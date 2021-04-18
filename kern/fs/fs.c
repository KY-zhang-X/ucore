#include <defs.h>
#include <kmalloc.h>
#include <sem.h>
#include <vfs.h>
#include <dev.h>
#include <file.h>
#include <sfs.h>
#include <inode.h>
#include <assert.h>
//called when init_main proc start
void
fs_init(void) {
    vfs_init();
    dev_init();
    sfs_init();
}

void
fs_cleanup(void) {
    vfs_cleanup();
}

/**
 * 互斥操作定义
 */ 
void
lock_files(struct files_struct *filesp) {
    down(&(filesp->files_sem));
}

void
unlock_files(struct files_struct *filesp) {
    up(&(filesp->files_sem));
}

/**
 * 创建进程时调用
 * 创建一个file_struct给该进程
 */
//Called when a new proc init
struct files_struct *
files_create(void) {
    //cprintf("[files_create]\n");
    static_assert((int)FILES_STRUCT_NENTRY > 128);
    struct files_struct *filesp;
    if ((filesp = kmalloc(sizeof(struct files_struct) + FILES_STRUCT_BUFSIZE)) != NULL) {
        /* 初始化成员变量 */
        filesp->pwd = NULL;
        filesp->fd_array = (void *)(filesp + 1);
        filesp->files_count = 0;
        sem_init(&(filesp->files_sem), 1);
        fd_array_init(filesp->fd_array);
    }
    return filesp;
}

/**
 * 进程退出时调用
 * 
 */ 
//Called when a proc exit
void
files_destroy(struct files_struct *filesp) {
//    cprintf("[files_destroy]\n");
    /* 只有当文件打开进程数量为0才能关闭进程，否则报错 */
    assert(filesp != NULL && files_count(filesp) == 0);
    if (filesp->pwd != NULL) {
        vop_ref_dec(filesp->pwd);/* 当前目录inode访问数量-1 */
    }
    int i;
    struct file *file = filesp->fd_array;
    for (i = 0; i < FILES_STRUCT_NENTRY; i ++, file ++) {
        if (file->status == FD_OPENED) {
            fd_array_close(file);/* 关闭此进程对文件的访问 */
        }
        assert(file->status == FD_NONE);//TODO
    }
    kfree(filesp);
}

//TODO
/**
 * 
 */ 
void
files_closeall(struct files_struct *filesp) {
//    cprintf("[files_closeall]\n");
    assert(filesp != NULL && files_count(filesp) > 0);
    int i;
    struct file *file = filesp->fd_array;
    //skip the stdin & stdout
    for (i = 2, file += 2; i < FILES_STRUCT_NENTRY; i ++, file ++) {
        if (file->status == FD_OPENED) {
            fd_array_close(file);
        }
    }
}


/**
 * 复制进程的文件数据结构file_struct
 */ 
int
dup_files(struct files_struct *to, struct files_struct *from) {
//    cprintf("[dup_files]\n");
    assert(to != NULL && from != NULL);
    assert(files_count(to) == 0 && files_count(from) > 0);
    if ((to->pwd = from->pwd) != NULL) {
        vop_ref_inc(to->pwd);
    }
    int i;
    struct file *to_file = to->fd_array, *from_file = from->fd_array;
    for (i = 0; i < FILES_STRUCT_NENTRY; i ++, to_file ++, from_file ++) {
        if (from_file->status == FD_OPENED) {
            /* alloc_fd first */
            to_file->status = FD_INIT;
            fd_array_dup(to_file, from_file);
        }
    }
    return 0;
}

