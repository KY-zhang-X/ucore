#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>


// open file in vfs, get/create inode for file with filename path.
/**
 * @brief vfs层open函数
 *        返回或者创建路径对应的inode
 *        openflag为读写权限
 * 
 */
int vfs_open(char *path, uint32_t open_flags, struct inode **node_store) 
{
    /* 设置读写权限 */
    bool can_write = 0;
    switch (open_flags & O_ACCMODE) {
    case O_RDONLY:
        break;
    case O_WRONLY:
    case O_RDWR:
        can_write = 1;
        break;
    default:
        return -E_INVAL;
    }

    if (open_flags & O_TRUNC) {
        if (!can_write) {
            return -E_INVAL;
        }
    }

    int ret; 
    struct inode *node;
    bool excl = (open_flags & O_EXCL) != 0;
    bool create = (open_flags & O_CREAT) != 0;
    ret = vfs_lookup(path, &node);

    if (ret != 0) {
        /* 如果文件不存在，判断create确定是否创建  */
        if (ret == -E_NOENT && (create)) {
            char *name;
            struct inode *dir;
            if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
                return ret;
            }
            /* 下层创建文件 */
            ret = vop_create(dir, name, excl, &node);
        } else return ret;
    } else if (excl && create) {
        /* 设置excl并以创建模式打开并且文件存在返回错误 */
        return -E_EXISTS;
    }
    assert(node != NULL);
    
    /* 下层打开文件 */
    if ((ret = vop_open(node, open_flags)) != 0) {
        vop_ref_dec(node);
        return ret;
    }

    vop_open_inc(node);
    /* 是否将创建（打开）文件将源文件覆盖 */
    if (open_flags & O_TRUNC || create) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            vop_open_dec(node);
            vop_ref_dec(node);
            return ret;
        }
    }
    *node_store = node;
    return 0;
}

// close file in vfs
int vfs_close(struct inode *node) 
{
    vop_open_dec(node);
    vop_ref_dec(node);
    return 0;
}

int vfs_unlink(char *path) {
    int ret;
    struct inode *dir;
    char *name;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
        return ret;
    }
    ret = vop_unlink(dir, name);
    vop_ref_dec(dir);
    return ret;
}

// unimplement
int vfs_rename(char *old_path, char *new_path) {
    return -E_UNIMP;
}

int vfs_link(char *old_path, char *new_path) {
    int ret;
    struct inode *dir;
    struct inode *node;
    char *name;
    if ((ret = vfs_lookup(old_path, &node)) != 0) {
        return ret;
    }
    if ((ret = vfs_lookup_parent(new_path, &dir, &name)) != 0) {
        vop_ref_dec(node);
        return ret;
    }
    ret = vop_link(dir, name, node);
    vop_ref_dec(dir);
    vop_ref_dec(node);
    return ret;
}

// unimplement
int vfs_symlink(char *old_path, char *new_path) {
    return -E_UNIMP;
}

// unimplement
int vfs_readlink(char *path, struct iobuf *iob) {
    return -E_UNIMP;
}

// 使用vop_mkdir实现
int vfs_mkdir(char *path) 
{
    int ret;
    struct inode *dir;
    char *name;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
        return ret;
    }
    ret = vop_mkdir(dir, name);
    vop_ref_dec(dir);
    return ret;
}
