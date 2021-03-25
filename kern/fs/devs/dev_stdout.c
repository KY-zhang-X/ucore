#include <defs.h>
#include <stdio.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

/**
 * 打开函数, 没有实际作用, 因为stdout对应console设备, 不存在
 * 对应struct device中的d_open函数
 * 
 * 检查stdout的只写属性
 **/
static int
stdout_open(struct device *dev, uint32_t open_flags) {
    if (open_flags != O_WRONLY) {
        return -E_INVAL;
    }
    return 0;
}

/**
 * 关闭函数, 没有实际作用
 * 对应struct device中的d_close函数
 **/
static int
stdout_close(struct device *dev) {
    return 0;
}

/**
 * io函数, 只有写没有读, 写对应到cputchar函数, 该函数实际上调用了console的输出函数cons_putc
 * 对应struct device中的d_io函数
 **/
static int
stdout_io(struct device *dev, struct iobuf *iob, bool write) {
    if (write) {
        char *data = iob->io_base;
        for (; iob->io_resid != 0; iob->io_resid --) {
            cputchar(*data ++);
        }
        return 0;
    }
    return -E_INVAL;
}

/**
 * io控制函数, 无实际作用
 * 对应struct device中的d_ioctl函数
 **/
static int
stdout_ioctl(struct device *dev, int op, void *data) {
    return -E_INVAL;
}

/**
 * 对stdout的struct device进行初始化
 * 被dev_init_stdout函数调用
 * 
 * 由于不是块设备, blocks数量为0, blocksize为1(表示读写的单位为1字节)
 **/
static void
stdout_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}

/**
 * 完成vfs中的设备stdout的初始化
 * 被dev_init调用
 * 
 * 在vfs中创建一个inode
 * 将inode中的in_info字段视为struct device进行初始化
 * 初始化后将该inode加入到vfs中的设备表vdef_list里
 **/
void
dev_init_stdout(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("stdout: dev_create_node.\n");
    }
    stdout_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdout", node, 0)) != 0) {
        panic("stdout: vfs_add_dev: %e.\n", ret);
    }
}

