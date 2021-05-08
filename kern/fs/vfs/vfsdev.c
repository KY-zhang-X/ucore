#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <dev.h>
#include <inode.h>
#include <sem.h>
#include <list.h>
#include <kmalloc.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

// device info entry in vdev_list 
/**
 * 此数据结构用双向链表将inode和device联通起来
 * ucore可以通过此链表访问所有的设备文件 
 * 可以找到device对应的inode
 */ 
typedef struct {
    const char *devname;
    struct inode *devnode;
    struct fs *fs;
    bool mountable;
    list_entry_t vdev_link;
} vfs_dev_t;

#define le2vdev(le, member)                         \
    to_struct((le), vfs_dev_t, member)

static list_entry_t vdev_list;     // vfs层设备链表
static semaphore_t vdev_list_sem;

static void
lock_vdev_list(void) {
    down(&vdev_list_sem);
}

static void
unlock_vdev_list(void) {
    up(&vdev_list_sem);
}

void
vfs_devlist_init(void) {
    list_init(&vdev_list);
    sem_init(&vdev_list_sem, 1);
}

// vfs_cleanup - finally clean (or sync) fs

/**
 * @brief 将文件系统同步到硬盘
 * 
 */
void
vfs_cleanup(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->fs != NULL) {
                    fsop_cleanup(vdev->fs);
                }
            }
        }
        unlock_vdev_list();
    }
}

/*
 * vfs_get_root - Given a device name (stdin, stdout, etc.), hand
 *                back an appropriate inode.
 */

/**
 * @brief 根据设备名称获取对应的root的inode
 * 
 * @param devname 设备名称
 * @param node_store 接受inode地址
 * @return int 成功返回0
 */
int
vfs_get_root(const char *devname, struct inode **node_store) {
    assert(devname != NULL);
    int ret = -E_NO_DEV;
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            /* 逐个查询设备链表 */
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (strcmp(devname, vdev->devname) == 0) {
                    struct inode *found = NULL;
                    if (vdev->fs != NULL) {
                        found = fsop_get_root(vdev->fs);
                    }
                    else if (!vdev->mountable) {
                        vop_ref_inc(vdev->devnode);
                        found = vdev->devnode;
                    }
                    if (found != NULL) {
                        ret = 0, *node_store = found;
                    }
                    else {
                        ret = -E_NA_DEV;
                    }
                    break;
                }
            }
        }
        unlock_vdev_list();
    }
    return ret;
}

/*
 * vfs_get_devname - Given a filesystem, hand back the name of the device it's mounted on.
 */

/**
 * @brief 获得文件系统挂载的设备名
 */
const char *
vfs_get_devname(struct fs *fs) {
    assert(fs != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->fs == fs) {
            return vdev->devname;
        }
    }
    return NULL;
}

/*
 * check_devname_confilct - Is there alreadily device which has the same name?
 */

/**
 * @brief 检查是否已存在devname的设备
 */
static bool
check_devname_conflict(const char *devname) {
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (strcmp(vdev->devname, devname) == 0) {
            return 0;
        }
    }
    return 1;
}


/**
 * @brief 在vfs层增加一个设备/文件(在设备链表增加一个表项)
 */
static int
vfs_do_add(const char *devname, struct inode *devnode, struct fs *fs, bool mountable) {
    assert(devname != NULL);
    assert((devnode == NULL && !mountable) || (devnode != NULL && check_inode_type(devnode, device)));
    if (strlen(devname) > FS_MAX_DNAME_LEN) {
        return -E_TOO_BIG;
    }

    int ret = -E_NO_MEM;
    char *s_devname;
    if ((s_devname = strdup(devname)) == NULL) {
        return ret;
    }

    vfs_dev_t *vdev;
    if ((vdev = kmalloc(sizeof(vfs_dev_t))) == NULL) {
        goto failed_cleanup_name;
    }

    ret = -E_EXISTS;
    lock_vdev_list();
    if (!check_devname_conflict(s_devname)) {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }
    vdev->devname = s_devname;
    vdev->devnode = devnode;
    vdev->mountable = mountable;
    vdev->fs = fs;

    list_add(&vdev_list, &(vdev->vdev_link));
    unlock_vdev_list();
    return 0;

failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_devname);
    return ret;
}

/*
 * Add a filesystem that does not have an underlying device.
 * This is used for emufs(not implementation???), but might also be used for network
 * filesystems and the like.
 */
/*
 * vfs_add_fs - 增加一个文件系统
 */
int
vfs_add_fs(const char *devname, struct fs *fs) {
    return vfs_do_add(devname, NULL, fs, 0);
}

/*
 * vfs_add_dev - 增加一个新的设备
 */
int
vfs_add_dev(const char *devname, struct inode *devnode, bool mountable) {
    return vfs_do_add(devname, devnode, NULL, mountable);
}

/*
 * find_mount - 找到设备名devname对应的挂载的设备，vdev_store存储表项
 */
static int
find_mount(const char *devname, vfs_dev_t **vdev_store) {
    assert(devname != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->mountable && strcmp(vdev->devname, devname) == 0) {
            *vdev_store = vdev;
            return 0;
        }
    }
    return -E_NO_DEV;
}

/*
 * vfs_mount - Mount a filesystem. Once we've found the device, call MOUNTFUNC to
 *             set up the filesystem and hand back a struct fs.
 *
 * The DATA argument is passed through unchanged to MOUNTFUNC.
 */

/**
 * @brief 
 * vfs层挂载一个设备（初始化设备）
 * 需要先调用vfs_do_add将设备加入设备链表
 */
int
vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store)) {
    int ret;
    lock_vdev_list();
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) {
        goto out;
    }
    if (vdev->fs != NULL) {
        ret = -E_BUSY;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    struct device *dev = vop_info(vdev->devnode, device);
    if ((ret = mountfunc(dev, &(vdev->fs))) == 0) {
        assert(vdev->fs != NULL);
        cprintf("vfs: mount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list();
    return ret;
}

/*
 * 通过设备名称卸载filesystem/device 
 * 先调用fsop_sync
 * 在调用fsop_unmount
 */
int
vfs_unmount(const char *devname) {
    int ret;
    lock_vdev_list();
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) {
        goto out;
    }
    if (vdev->fs == NULL) {
        ret = -E_INVAL;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    /* 将未保存数据写回磁盘 */
    if ((ret = fsop_sync(vdev->fs)) != 0) { 
        goto out;
    }

    /* 调用对应的卸载函数 */
    if ((ret = fsop_unmount(vdev->fs)) == 0) {
        vdev->fs = NULL;
        cprintf("vfs: unmount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list();
    return ret;
}

/*
 * 卸载vfs层所有设备
 * 和vfs_unmount类似
 */
int
vfs_unmount_all(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            /* 遍历设备链表，逐个卸载 */ 
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->mountable && vdev->fs != NULL) {
                    int ret;
                    if ((ret = fsop_sync(vdev->fs)) != 0) {
                        cprintf("vfs: warning: sync failed for %s: %e.\n", vdev->devname, ret);
                        continue ;
                    }
                    if ((ret = fsop_unmount(vdev->fs)) != 0) {
                        cprintf("vfs: warning: unmount failed for %s: %e.\n", vdev->devname, ret);
                        continue ;
                    }
                    vdev->fs = NULL;
                    cprintf("vfs: unmount %s.\n", vdev->devname);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}

