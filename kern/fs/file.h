#ifndef __KERN_FS_FILE_H__
#define __KERN_FS_FILE_H__

//#include <types.h>
#include <fs.h>
#include <proc.h>
#include <atomic.h>
#include <assert.h>

struct inode;
struct stat;
struct dirent;


/**
 * 指定文件文件系统的相关类型，包括读写权限，文件描述符fd,当前读到的位置POS
 * 文件系统中与硬盘特定区域所对应的节点node，以及打开此文件次数open_count
 */ 
struct file {
    enum {
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    } status;                   //访问文件的执行状态
    bool readable;              //文件是否可读
    bool writable;              //文件是否可写
    int fd;                     //文件在filemap中的索引值
    off_t pos;                  //访问文件的当前位置
    struct inode *node;         //读文件对应的内存指针inode
    int open_count;             //打开此文件的次数
};

void fd_array_init(struct file *fd_array);
void fd_array_open(struct file *file);
void fd_array_close(struct file *file);
void fd_array_dup(struct file *to, struct file *from);
bool file_testfd(int fd, bool readable, bool writable);

int file_open(char *path, uint32_t open_flags);
int file_close(int fd);
int file_read(int fd, void *base, size_t len, size_t *copied_store);
int file_write(int fd, void *base, size_t len, size_t *copied_store);
int file_seek(int fd, off_t pos, int whence);
int file_fstat(int fd, struct stat *stat);
int file_fsync(int fd);
int file_getdirentry(int fd, struct dirent *dirent);
int file_dup(int fd1, int fd2);
int file_pipe(int fd[]);
int file_mkfifo(const char *name, uint32_t open_flags);

/**
 * 返回打开此文件的次数
 */ 
static inline int
fopen_count(struct file *file) {
    return file->open_count;
}


/**
 * 打开此文件的次数-1
 * 返回打开此文件的次数
 */ 
static inline int
fopen_count_inc(struct file *file) {
    file->open_count += 1;
    return file->open_count;
}

/**
 * 打开此文件的次数-1
 * 返回打开此文件的次数
 */ 
static inline int
fopen_count_dec(struct file *file) {
    file->open_count -= 1;
    return file->open_count;
}

#endif /* !__KERN_FS_FILE_H__ */

