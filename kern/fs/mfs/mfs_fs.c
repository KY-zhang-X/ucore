#include <defs.h>
#include <kmalloc.h>
#include <dev.h>
#include <vfs.h>
#include <mfs.h>


/* 将mfs的superblock和 */
static int mfs_sync(struct fs *fs)
{

}


static struct inode *mfs_get_root(struct fs *fs)
{
    
}

static int mfs_unmount(struct fs *fs)
{

}

static void mfs_cleanup(struct fs *fs)
{

}


static int mfs_do_mount(struct device *dev, struct fs **fs_store)
{
    if (dev->d_blocksize != MFS_BLKSIZE) {
        return -E_NA_DEV;
    }

    /* 为fs结构体分配内存空间 */
    struct fs *fs;
    if ((fs = alloc_fs(mfs)) == NULL) {
        return -E_NO_MEM;
    }

    /* 将fs结构体中的fs_info字段视为mfs_fs结构体, 与设备结构体关联 */
    struct mfs_fs *mfs = fsop_info(fs, mfs);
    mfs->dev = dev;

    int ret = -E_NO_MEM;

    /* 为mfs_fs结构体中的buffer分配内存空间 */
    void *mfs_buffer;
    if ((mfs->mfs_buffer = mfs_buffer = kmalloc(MFS_BLKSIZE)) == NULL) {
        goto failed_cleanup_fs;
    }

    /* 读块设备的超级块到buffer中 */
    if ((ret = sfs_init_read(dev, MFS_BLKN_SUPER, mfs_buffer)) != 0) {
        goto failed_cleanup_mfs_buffer;
    }

    ret = -E_INVAL;

    /* 检查超级块是否满足mfs的要求, 满足则将mfs的super指针指向这个区域 */
    struct mfs_super *super = mfs_buffer;
    if (super->magic != MFS_MAGIC) {
        cprintf("mfs: wrong magic in superblock. (%04x should be %04x).\n",
            super->magic, MFS_MAGIC);
        goto failed_cleanup_mfs_buffer;
    }
    uint32_t blocks = super->nzones << super->log_zone_size;
    if (blocks > dev->d_blocks) {
        cprintf("mfs: fs has %u blocks, device has %u blocks.\n",
                blocks, dev->d_blocks);
        goto failed_cleanup_sfs_buffer;
    }
    mfs->super = *super;

    ret = -E_NO_MEM;

    uint32_t i;

    /* 为哈希表分配空间并初始化 */
    list_entry_t *hash_list;
    if ((mfs->hash_list = hash_list = kmalloc(sizeof(list_entry_t) * MFS_HLIST_SIZE)) == NULL) {
        goto failed_cleanup_mfs_buffer;
    }
    for (i = 0; i < MFS_HLIST_SIZE; i++) {
        list_init(hash_list + i);
    }

    //TODO: inode bitmap初始化
    struct bitmap *inodemap;
    // uint32_t imap_size_nbits = super->imap_blocks * MFS_BLKSIZE;
    // if ((mfs->inodemap = inodemap = bitmap_create(imap_size_nbits)) == NULL) {
    //     goto failed_cleanup_hash_list;
    // }
    for (i = 0; i < super->imap_blocks; i++) {
        if (mfs->inodemap[i] = inodemap = bitmap_create(MFS_BLKBITS)) {
            goto failed_cleanup_inodemap;   
        }
    }
    

    //TODO: zone bitmap初始化
    // struct bitmap *zonemap;
    // uint32_t zmap_size_nbits = super->zmap_blocks * MFS_BLKSIZE;
    // if ((mfs->zonemap = zonemap = bitmap_create(zmap_size_nbits)) == NULL) {
    //     goto failed_cleanup_inodemap;
    // }
    

    /* 其他字段初始化 */
    mfs->super_dirty = 0;
    mfs->super_rdonly = 0;
    sem_init(&(mfs->io_sem), 1);
    sem_init(&(mfs->fs_sem), 1);
    sem_init(&(mfs->mutex_sem), 1);
    list_init(&(mfs->inode_list));
    // cprintf("mfs: mount: '%s' ");

    fs->fs_sync = mfs_sync;
    fs->fs_get_root = mfs_get_root;
    fs->fs_unmount = mfs_unmount;
    fs->fs_cleanup = mfs_cleanup;
    *fs_store = fs;
    return 0;

// failed_cleanup_freemap:
failed_cleanup_hash_list:
    kfree(hash_list);
failed_cleanup_mfs_buffer:
    kfree(mfs_buffer);
failed_cleanup_fs:
    kfree(fs);
    return ret;
}

int mfs_mount(const char *devname)
{
    return vfs_mount(devname, mfs_do_mount);
}
