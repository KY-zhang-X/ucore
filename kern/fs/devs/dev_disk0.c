#include <defs.h>
#include <mmu.h>
#include <sem.h>
#include <ide.h>
#include <inode.h>
#include <kmalloc.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

#define DISK0_BLKSIZE                   PGSIZE
#define DISK0_BUFSIZE                   (4 * DISK0_BLKSIZE)
#define DISK0_BLK_NSECT                 (DISK0_BLKSIZE / SECTSIZE)

static char *disk0_buffer;
static semaphore_t disk0_sem;

static void
lock_disk0(void) {
    down(&(disk0_sem));
}

static void
unlock_disk0(void) {
    up(&(disk0_sem));
}

/**
 * 打开函数, 没有实际作用, 因为disk0作为一个磁盘始终是打开的
 * 对应 struct device 中的 d_open 函数
 **/
static int
disk0_open(struct device *dev, uint32_t open_flags) {
    return 0;
}

/**
 * 关闭函数, 没有实际作用, 因为disk0作为一个磁盘不存在关闭的说法
 * 对应 struct device 中的 d_close 函数
 **/
static int
disk0_close(struct device *dev) {
    return 0;
}

/** 
 * 从磁盘扇区按块读取到内存, 无锁
 * 在 disk0_io 函数中被调用
 * 
 * 读取从 @blkno 开始的 @nblks 个块到 @disk0_buffer
 * 
 * 块大小为 DISK0_BLKSIZE = PGSIZE = 4096
 * 扇区大小为 SECTSIZE = 512
 * disk0_buffer大小为 DISK0_BUFSIZE = 4 * DISK0_BLKSIZE
 **/
static void
disk0_read_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT;
    if ((ret = ide_read_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0) {
        panic("disk0: read blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sectno, nblks, nsecs, ret);
    }
}

/**
 * 从内存按块写入到磁盘扇区, 无锁
 * 在 disk0_io 函数中被调用
 * 
 * 从 @disk_buffer 写入到磁盘从 @blkno 开始的 @nblks 个块
 * 写入失败会给出panic, 但没有返回值
 **/
static void
disk0_write_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT;
    if ((ret = ide_write_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0) {
        panic("disk0: write blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sectno, nblks, nsecs, ret);
    }
}

/**
 * disk0的io操作接口实现
 * 对应 struct device 中的 d_io 函数
 * 
 * 根据 @write 的值确定方向, 完成 @iob 和 @disk_buffer 的数据交换
 * 然后，调用磁盘块读写函数将数据持久化到磁盘中
 * 
 * 要求iobuf的偏移量和剩余长度必须是整数个块
 **/
static int
disk0_io(struct device *dev, struct iobuf *iob, bool write) {
    off_t offset = iob->io_offset;
    size_t resid = iob->io_resid;
    uint32_t blkno = offset / DISK0_BLKSIZE;
    uint32_t nblks = resid / DISK0_BLKSIZE;

    /* don't allow I/O that isn't block-aligned */
    if ((offset % DISK0_BLKSIZE) != 0 || (resid % DISK0_BLKSIZE) != 0) {
        return -E_INVAL;
    }

    /* don't allow I/O past the end of disk0 */
    if (blkno + nblks > dev->d_blocks) {
        return -E_INVAL;
    }

    /* read/write nothing ? */
    if (nblks == 0) {
        return 0;
    }

    lock_disk0();
    while (resid != 0) {
        size_t copied, alen = DISK0_BUFSIZE;
        if (write) {
            iobuf_move(iob, disk0_buffer, alen, 0, &copied);
            assert(copied != 0 && copied <= resid && copied % DISK0_BLKSIZE == 0);
            nblks = copied / DISK0_BLKSIZE;
            disk0_write_blks_nolock(blkno, nblks);
        }
        else {
            if (alen > resid) {
                alen = resid;
            }
            nblks = alen / DISK0_BLKSIZE;
            disk0_read_blks_nolock(blkno, nblks);
            iobuf_move(iob, disk0_buffer, alen, 1, &copied);
            assert(copied == alen && copied % DISK0_BLKSIZE == 0);
        }
        resid -= copied, blkno += nblks;
    }
    unlock_disk0();
    return 0;
}


/**
 * io控制函数, 没有实际作用
 * 对应 struct device 中的 d_ioctl 函数
 **/
static int
disk0_ioctl(struct device *dev, int op, void *data) {
    return -E_UNIMP;
}

/**
 * 对disk0的struct device进行初始化
 * 被dev_init_disk0调用
 * 
 * 初始化struct device的各项内容
 * 初始化设备互斥信号量
 * 为disk0缓冲区disk0_buffer分配空间
 **/
static void
disk0_device_init(struct device *dev) {
    static_assert(DISK0_BLKSIZE % SECTSIZE == 0);
    if (!ide_device_valid(DISK0_DEV_NO)) {
        panic("disk0 device isn't available.\n");
    }
    dev->d_blocks = ide_device_size(DISK0_DEV_NO) / DISK0_BLK_NSECT;
    dev->d_blocksize = DISK0_BLKSIZE;
    dev->d_open = disk0_open;
    dev->d_close = disk0_close;
    dev->d_io = disk0_io;
    dev->d_ioctl = disk0_ioctl;
    sem_init(&(disk0_sem), 1);

    static_assert(DISK0_BUFSIZE % DISK0_BLKSIZE == 0);
    if ((disk0_buffer = kmalloc(DISK0_BUFSIZE)) == NULL) {
        panic("disk0 alloc buffer failed.\n");
    }
}

/**
 * 完成vfs中的device的初始化
 * 被dev_init调用
 * 
 * 在vfs中为disk0创建一个inode
 * 将struct inode中的in_info域看作struct device进行初始化
 * 完成后将inode加入到vfs的device表中
 **/
void
dev_init_disk0(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("disk0: dev_create_node.\n");
    }
    disk0_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("disk0", node, 1)) != 0) {
        panic("disk0: vfs_add_dev: %e.\n", ret);
    }
}

