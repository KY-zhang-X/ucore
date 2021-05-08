#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <iobuf.h>
#include <stat.h>
#include <proc.h>
#include <error.h>
#include <assert.h>

/*
 * get_cwd_nolock - retrieve current process's working directory. without lock protect
 */

/**
 * 返回当前进程的工作目录，无锁保护
 */
static struct inode *
get_cwd_nolock(void) {
    return current->filesp->pwd;
}

/**
 * 改变当前进程的工作目录，无锁保护
 */
static void
set_cwd_nolock(struct inode *pwd) {
    current->filesp->pwd = pwd;
}

/*
 * lock_cfs - lock the fs related process on current process 
 */
/**
 * 对当前进程的文件打开表加锁
 */ 
static void
lock_cfs(void) {
    lock_files(current->filesp);
}
/*
 * unlock_cfs - unlock the fs related process on current process 
 */
/**
 * 对当前进程的文件打开表解锁
 */ 
static void
unlock_cfs(void) {
    unlock_files(current->filesp);
}

/*
 *  vfs_get_curdir - Get current directory as a inode.
 */

/**
 * 获得当前当前目录的inode节点
 */ 
int
vfs_get_curdir(struct inode **dir_store) {
    struct inode *node;
    if ((node = get_cwd_nolock()) != NULL) {
        vop_ref_inc(node);
        *dir_store = node;
        return 0;
    }
    return -E_NOENT;
}

/*
 * vfs_set_curdir - Set current directory as a inode.
 *                  The passed inode must in fact be a directory.
 */
/**
 * 改变工作目录(inode层面)
 * 传入的inode必须是目录节点
 */  
int
vfs_set_curdir(struct inode *dir) {
    int ret = 0;
    lock_cfs();
    struct inode *old_dir;
    if ((old_dir = get_cwd_nolock()) != dir) {
        if (dir != NULL) {
            uint32_t type;
            if ((ret = vop_gettype(dir, &type)) != 0) {
                goto out;
            }
            /* 判断新inode是否为目录节点 */
            if (!S_ISDIR(type)) {
                ret = -E_NOTDIR;
                goto out;
            }
            vop_ref_inc(dir);
        }
        set_cwd_nolock(dir);
        if (old_dir != NULL) {
            /* 旧目录引用计数-1 */
            vop_ref_dec(old_dir);
        }
    }
out:
    unlock_cfs();
    return ret;
}

/**
 * 改变当前进程工作目录(vfs层顶层接口)
 * 用vfs_lookup将目录转换为inode
 */
int
vfs_chdir(char *path) {
    int ret;
    struct inode *node;
    /* 先找出path对应inode */
    if ((ret = vfs_lookup(path, &node)) == 0) {
        ret = vfs_set_curdir(node);
        vop_ref_dec(node);
    }
    return ret;
}
/*
 * vfs_getcwd - retrieve current working directory(cwd).
 */
/**
 * 返回当前进程工作目录(vfs层最高级接口)
 * 通过iob返回路径
 */ 
int
vfs_getcwd(struct iobuf *iob) {
    int ret;
    struct inode *node;
    /* 获得cwd的inode */
    if ((ret = vfs_get_curdir(&node)) != 0) {
        return ret;
    }
    assert(node->in_fs != NULL);

    /* 获得设备名称 */
    const char *devname = vfs_get_devname(node->in_fs);
    if ((ret = iobuf_move(iob, (char *)devname, strlen(devname), 1, NULL)) != 0) {
        goto out;
    }
    char colon = ':';
    if ((ret = iobuf_move(iob, &colon, sizeof(colon), 1, NULL)) != 0) {
        goto out;
    }
    /* 得到inode相对于根目录的路径并写入iob */
    ret = vop_namefile(node, iob);

out:
    vop_ref_dec(node);
    return ret;
}

