#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>
#include <assert.h>

/*
 * get_device- Common code to pull the device name, if any, off the front of a
 *             path and choose the inode to begin the name lookup relative to.
 */

/**
 * get_device 获得设备\文件系统的根节点对应inode以及相对根节点的路径
 * 
 */
static int
get_device(char *path, char **subpath, struct inode **node_store) {
    int i, slash = -1, colon = -1;
    for (i = 0; path[i] != '\0'; i ++) {
        if (path[i] == ':') { colon = i; break; }
        if (path[i] == '/') { slash = i; break; }
    }
    if (colon < 0 && slash != 0) {
        /* *
         * No colon before a slash, so no device name specified, and the slash isn't leading
         * or is also absent, so this is a relative path or just a bare filename. Start from
         * the current directory, and use the whole thing as the subpath.
         * */
        /*
        * slash(/)前面没有colon(:)，且第一个字符不是(/),没有具体的设备，是相对路径或者文件名
        */
        *subpath = path;
        return vfs_get_curdir(node_store);
    }
    if (colon > 0) {
        /* 格式为device:path, 有colon(:)，返回设备根目录inode */
        path[colon] = '\0';

        /* 格式也可能为device:/path，跳过(/)  */
        while (path[++ colon] == '/');
        *subpath = path + colon;
        return vfs_get_root(path, node_store);
    }

    /* *
     * 其余两种文件格式
     * /path 相对于主文件系统的路径
     * :path 相对于当前（设备）文件系统的路径 
     * */
    int ret;
    if (*path == '/') {
        /* 返回主文件系统根节点inode */
        if ((ret = vfs_get_bootfs(node_store)) != 0) {
            return ret;
        }
    }
    else {
        assert(*path == ':');
        struct inode *node;
        /* 获得cwd的inode */
        if ((ret = vfs_get_curdir(&node)) != 0) {
            return ret;
        }
        /* 当前目录可能不是设备，所以检查node->infs */
        assert(node->in_fs != NULL);

        /* 返回设备的根节点 */
        *node_store = fsop_get_root(node->in_fs);
        vop_ref_dec(node);
    }

    /* ///... or :/... */
    while (*(++ path) == '/');
    *subpath = path;
    return 0;
}

/*
 * vfs_lookup - get the inode according to the path filename
 */
/**
 * 通过路径获得对应目录/文件的inode
 */ 
int
vfs_lookup(char *path, struct inode **node_store) {
    int ret;
    struct inode *node;
    /* 获得设备/文件系统/相对路径根目录 */
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    if (*path != '\0') {
        /* 根据根目录inode找子目录的inode */
        ret = vop_lookup(node, path, node_store);
        vop_ref_dec(node);
        return ret;
    }
    *node_store = node;
    return 0;
}

/* 
 * 获得设备/文件系统/相对路径根目录对应inode ,以及相对根目录的路径 
 * 下层调用了get_device
 */
int
vfs_lookup_parent(char *path, struct inode **node_store, char **endp){
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    *endp = path;
    *node_store = node;
    return 0;
}
