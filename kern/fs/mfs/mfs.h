#ifndef __KERN_FS_MFS_H__
#define __KERN_FS_MFS_H__

#include <defs.h>
#include <list.h>
#include <sem.h>


#define MFS_MAGIC           0x137f
#define MFS_BLKSIZE         1024
#define MFS_NDIRECT         7
#define MFS_MAX_FNAME_LEN   13
#define MFS_BLKN_SUPER      1

/* 1逻辑块中的bits数 */
#define MFS_BLKBITS         (MFS_BLKSIZE * CHAR_BIT)

/* 1逻辑块中的项目数 */
// #define MFS_BLK_NENTRY                              (MFS_BLKSIZE / sizeof(uint32_t))

/* 文件类型 */
#define MFS_TYPE_FIFO   1       // FIFO文件
#define MFS_TYPE_CDEV   2       // 字符设备文件
#define MFS_TYPE_DIR    4       // 目录文件
#define MFS_TYPE_BDEV   6       // 块设备文件
#define MFS_TYPE_FILE   8       // 常规文件

/**
 * 磁盘上的超级块(SuperBlock)
 **/
struct mfs_super {
    uint16_t ninodes;         // inode数目
    uint16_t nzones;          // 逻辑块数
    uint16_t imap_blocks;     // inode位图所占块数
    uint16_t zmap_blocks;     // 逻辑块位图所占块数
    uint16_t firstdatazone;   // 数据区中第一个逻辑块块号
    uint16_t log_zone_size;   // Log2(磁盘块数/逻辑块数)
    uint32_t max_size;        // 最大文件长度
    uint16_t magic;           // magic number
};

/**
 * 磁盘上的inode
 **/
struct mfs_disk_inode {
    uint16_t mode;                // 文件的类型和属性(rwx), 这里只用到类型字段
    uint16_t uid;                 // 文件所有者的用户id, 不使用
    uint32_t size;                // 文件长度(字节)
    uint32_t mtime;               // 最后修改时间(秒)
    uint8_t  gid;                 // 文件所有者的组id, 不使用
    uint8_t  nlinks;              // 硬链接数
    uint16_t direct[MFS_NDIRECT]; // 直接块号
    uint16_t indirect;            // 一级间接块号
    uint16_t db_indirect;         // 二级间接块号
};

/**
 * 磁盘上的文件项目
 **/
struct mfs_disk_entry {
    uint16_t ino;                       // inode号
    char name[MFS_MAX_FNAME_LEN + 1];   // 文件名
};

#define mfs_dentry_size                             \
    sizeof(((struct mfs_disk_entry *)0)->name)

/**
 * 内存中的inode
 **/
struct mfs_inode {
    struct mfs_disk_inode *din;     // 磁盘上的inode
    uint32_t no;                   // 节点号 (linux-0.11中数据类型是uint16_t)
    // uint32_t atime;               // 最后访问时间
    // uint32_t ctime;               // inode自身被修改时间
    uint16_t count;               // inode被引用的次数
    uint8_t dirty;               // inode已被修改标志
    uint8_t pipe;                // inode用作管道标志
    semaphore_t sem;              // inode信号量
    list_entry_t inode_link;      // mfs_fs中链表的项目
    list_entry_t hash_link;       // mfs_fs中哈希表的项目
};

#define le2min(le, member)                  \
    to_struct((le), struct mfs_inode, member)

/**
 * 内存中的MFS文件系统
 **/
struct mfs_fs {
    struct mfs_super super;         // 磁盘上的superblock
    struct device *dev;             // 设备
    struct bitmap *inodemap[8];     // inode位图
    struct bitmap *zonemap[8];      // 逻辑块位图
    struct mfs_inode *isup;         // 该安装文件系统根目录inode
    struct mfs_inode *imount;       // 该文件系统被安装到的inode
    uint8_t super_dirty;            // 已被修改标志
    uint8_t super_rdonly;           // 只读标志
    void *mfs_buffer;               // 缓冲区
    semaphore_t fs_sem;             // fs使用的信号量
    semaphore_t io_sem;             // io使用的信号量
    semaphore_t mutex_sem;          // 建立链接/释放链接的信号量
    list_entry_t inode_list;        // inode链表
    list_entry_t *hash_list;        // inode哈希表
};

/* MFS文件系统的哈希表相关定义 */
#define MFS_HLIST_SHIFT                             10
#define MFS_HLIST_SIZE                              (1 << MFS_HLIST_SHIFT)
#define min_hashfn(x)                               (hash32(x, MFS_HLIST_SHIFT))

/* MFS的操作 */
void mfs_init(void);                    // 初始化MFS文件系统, 调用mfs_mount函数
int mfs_mount(const char *devname);     // 挂载devname对应的设备, 将该设备中的文件系统视为mfs进行初始化

void lock_mfs_fs(struct mfs_fs *mfs);
void lock_mfs_io(struct mfs_fs *mfs);
void unlock_mfs_fs(struct mfs_fs *mfs);
void unlock_mfs_io(struct mfs_fs *mfs);

int mfs_rblock(struct mfs_fs *mfs, void *buf, uint32_t blkno, uint32_t nblks);
int mfs_wblock(struct mfs_fs *mfs, void *buf, uint32_t blkno, uint32_t nblks);
int mfs_rbuf(struct mfs_fs *mfs, void *buf, size_t len, uint32_t blkno, off_t offset);
int mfs_wbuf(struct mfs_fs *mfs, void *buf, size_t len, uint32_t blkno, off_t offset);
int mfs_sync_super(struct mfs_fs *mfs);
int mfs_clear_block(struct mfs_fs *mfs, uint32_t blkno, uint32_t nblks);

int mfs_load_inode(struct mfs_fs *mfs, struct inode **node_store, uint32_t ino);

#endif /* !__KERN_FS_MFS_H__ */