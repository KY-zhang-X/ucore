#include <defs.h>
#include <stdio.h>
#include <wait.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

#define STDIN_BUFSIZE               4096

static char stdin_buffer[STDIN_BUFSIZE]; // stdin缓冲区
static off_t p_rpos, p_wpos; // read position 和 write position 对应向缓冲区里读和写的位置
static wait_queue_t __wait_queue, *wait_queue = &__wait_queue; // 等待队列, 存储阻塞在stdin上的进程


/**
 * 写stdin缓冲区stdin_buffer
 * 在<trap.c>中的键盘中断处被调用
 * 
 * 输入缓冲区大小为 STDIN_BUFSIZE = 4096
 * 如果stdin_buffer缓冲的字符数达到上限, 则最后写入的字符会持续被覆盖
 * 写字符的时候会检测等待队列是否为空, 如果不为空就唤醒其中所有进程 (?: 唤醒其中所有进程)
 * 上述操作均在中断的临界区内完成
 * (p_rpos和p_wpos都是一直向前增长, 但是由于都采用了off_t即64位整数, 因此不需要担心溢出的问题)
 **/
void
dev_stdin_write(char c) {
    bool intr_flag;
    if (c != '\0') {
        local_intr_save(intr_flag);
        {
            stdin_buffer[p_wpos % STDIN_BUFSIZE] = c;
            if (p_wpos - p_rpos < STDIN_BUFSIZE) {
                p_wpos ++;
            }
            if (!wait_queue_empty(wait_queue)) {
                wakeup_queue(wait_queue, WT_KBD, 1);
            }
        }
        local_intr_restore(intr_flag);
    }
}

/**
 * 读stdin缓冲区stdin_buffer
 * 在stdin_io处被调用
 * 
 * 从stdin缓冲区stdin_buffer中读长度为len的数据到缓冲区buf中
 * 如果缓冲区内没有内容, 则调用wait_current_set将当前进程入队, 进程进入阻塞状态, 同时开启调度
 * 当进程阻塞状态解除时, 检查wakeup_flags是否为WT_KBD, 符合则重新尝试从缓冲区中取数据, 否则退出
 * 返回值是成功读取的字符个数
 **/
static int
dev_stdin_read(char *buf, size_t len) {
    int ret = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        for (; ret < len; ret ++, p_rpos ++) {
        try_again:
            if (p_rpos < p_wpos) {
                *buf ++ = stdin_buffer[p_rpos % STDIN_BUFSIZE];
            }
            else {
                wait_t __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                local_intr_restore(intr_flag);

                schedule();

                local_intr_save(intr_flag);
                wait_current_del(wait_queue, wait);
                if (wait->wakeup_flags == WT_KBD) {
                    goto try_again; // 使用goto的目的是, 防止执行for循环的ret++, p_rpos++语句
                }
                break;
            }
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}

/**
 * 打开函数, 没有实际作用
 * 对应struct device中的d_open函数
 * 
 * 检查stdin的只读标志
 **/
static int
stdin_open(struct device *dev, uint32_t open_flags) {
    if (open_flags != O_RDONLY) {
        return -E_INVAL;
    }
    return 0;
}

/**
 * 关闭函数, 没有实际作用
 * 对应struct device中的d_close函数
 **/
static int
stdin_close(struct device *dev) {
    return 0;
}

/**
 * 设备io接口函数, 只有读没有写
 * 对应struct device中的d_io函数
 * 
 * 调用dev_stdin_read将stdin_buffer中的缓冲发送到iobuf中
 * 返回实际读取到的字节数
 **/
static int
stdin_io(struct device *dev, struct iobuf *iob, bool write) {
    if (!write) {
        int ret;
        if ((ret = dev_stdin_read(iob->io_base, iob->io_resid)) > 0) {
            iob->io_resid -= ret;
        }
        return ret;
    }
    return -E_INVAL;
}

/**
 * 设备io控制接口函数, 没有实际作用
 * 对应struct device中的d_ioctl函数
 **/
static int
stdin_ioctl(struct device *dev, int op, void *data) {
    return -E_INVAL;
}

/**
 * 初始化stdin的设备结构体, 以及stdin缓冲区和等待队列
 * 在dev_init_stdin处被调用
 **/
static void
stdin_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdin_open;
    dev->d_close = stdin_close;
    dev->d_io = stdin_io;
    dev->d_ioctl = stdin_ioctl;

    p_rpos = p_wpos = 0;
    wait_queue_init(wait_queue);
}

/**
 * 初始化在vfs中的stdin设备
 * 在dev_init中被调用
 * 
 * 为stdin创建一个inode, 将inode中的in_info字段作为struct device初始化
 * 将inode加入到vfs的设备列表vdef_list中
 **/
void
dev_init_stdin(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("stdin: dev_create_node.\n");
    }
    stdin_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdin", node, 0)) != 0) {
        panic("stdin: vfs_add_dev: %e.\n", ret);
    }
}

