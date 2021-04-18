#include <defs.h>
#include <sfs.h>
#include <error.h>
#include <assert.h>

/*
 * sfs_init - mount sfs on disk0
 *
 * CALL GRAPH:
 *   kern_init-->fs_init-->sfs_init
 *
 * 挂载disk0中的sfs文件系统
 */
void
sfs_init(void) {
    int ret;
    if ((ret = sfs_mount("disk0")) != 0) {
        panic("failed: sfs: sfs_mount: %e.\n", ret);
    }
}

