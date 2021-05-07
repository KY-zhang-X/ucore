#include <defs.h>
#include <mfs.h>
#include <error.h>
#include <assert.h>

void mfs_init(void) {
    int ret;
    if ((ret = mfs_mount("disk1")) != 0) {
        panic("failed: mfs: mfs_mount: %e.\n", ret);
    }
}