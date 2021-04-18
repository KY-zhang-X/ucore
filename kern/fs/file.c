#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <proc.h>
#include <file.h>
#include <unistd.h>
#include <iobuf.h>
#include <inode.h>
#include <stat.h>
#include <dirent.h>
#include <error.h>
#include <assert.h>

/* fd条件范围判断 */
#define testfd(fd)                          ((fd) >= 0 && (fd) < FILES_STRUCT_NENTRY)



// get_fd_array - get current process's open files tabl
/*
 * 返回当前执行进程的文件打开表
 *  
 */
static struct file *
get_fd_array(void) {
    struct files_struct *filesp = current->filesp;
    assert(filesp != NULL && files_count(filesp) > 0);
    return filesp->fd_array;
}

 
// fd_array_init - initialize the open files table
/**
 * 在进程文件数据结构初始化时调用files_create，也是进程初始化时调用
 * 初始化进程的文件打开表
 * 把所有表中所有文件状态写为FD_NONE
 */
void
fd_array_init(struct file *fd_array) {
    int fd;
    struct file *file = fd_array;
    for (fd = 0; fd < FILES_STRUCT_NENTRY; fd ++, file ++) {
        file->open_count = 0;
        file->status = FD_NONE, file->fd = fd;
    }
}


// fs_array_alloc - allocate a free file item (with FD_NONE status) in open files table

/**
 * @brief 给当前进程的文件打开表分配一个表项
 * 
 * @param fd 文件描述符
 * @param file_store 分配地址
 * @return 
 */
static int
fd_array_alloc(int fd, struct file **file_store) {
//    panic("debug");
    struct file *file = get_fd_array();
    if (fd == NO_FD) {
        for (fd = 0; fd < FILES_STRUCT_NENTRY; fd ++, file ++) {
            if (file->status == FD_NONE) {
                goto found;
            }
        }
        return -E_MAX_OPEN;
    }
    else {
        if (testfd(fd)) {
            file += fd;
            if (file->status == FD_NONE) {
                goto found;
            }
            return -E_BUSY;
        }
        return -E_INVAL;
    }
found:
    assert(fopen_count(file) == 0);
    file->status = FD_INIT, file->node = NULL;
    *file_store = file;
    return 0;
}
/**
 * 释放一个文件的内存空间
 */ 
// fd_array_free - free a file item in open files table
static void
fd_array_free(struct file *file) {
    assert(file->status == FD_INIT || file->status == FD_CLOSED);
    assert(fopen_count(file) == 0);
    if (file->status == FD_CLOSED) {
        vfs_close(file->node);
    }
    file->status = FD_NONE;
}

/**
 * @brief 请求访问文件，该文件的open_count++
 * 
 * @param file 
 */
static void
fd_array_acquire(struct file *file) {
    assert(file->status == FD_OPENED); /* 这个函数是读文件调用，先打开再读 */
    fopen_count_inc(file);
}

/**
 * @brief 取消对文件的访问，该文件的open_count--,如果open_count为0,即访问该文件的进程为0时调用fd_array_free释放该文件的内存空间。
 * 
 */
// fd_array_release - file's open_count--; if file's open_count-- == 0 , then call fd_array_free to free this file item
static void
fd_array_release(struct file *file) {
    assert(file->status == FD_OPENED || file->status == FD_CLOSED);
    assert(fopen_count(file) > 0);
    if (fopen_count_dec(file) == 0) {
        fd_array_free(file);
    }
}


// fd_array_open - file's open_count++, set status to FD_OPENED
/**
 * 打开文件
 * file->open_count++
 * file->status = FD_OPENED;
 */ 
void
fd_array_open(struct file *file) {
    assert(file->status == FD_INIT && file->node != NULL);
    file->status = FD_OPENED;
    fopen_count_inc(file);
}


// fd_array_close - file's open_count--; if file's open_count-- == 0 , then call fd_array_free to free this file item
/**
 * 关闭一个文件
 * 如果此文件的全局文件打开表为0
 * 则释放此文件的内存空间
 */ 
void
fd_array_close(struct file *file) {
    assert(file->status == FD_OPENED);
    assert(fopen_count(file) > 0);
    file->status = FD_CLOSED;
    if (fopen_count_dec(file) == 0) {
        fd_array_free(file);
    }
}


/**
 * 
 */ 
//fs_array_dup - duplicate file 'from'  to file 'to'
void
fd_array_dup(struct file *to, struct file *from) {
    //cprintf("[fd_array_dup]from fd=%d, to fd=%d\n",from->fd, to->fd);
    assert(to->status == FD_INIT && from->status == FD_OPENED);
    to->pos = from->pos;
    to->readable = from->readable;
    to->writable = from->writable;
    struct inode *node = from->node;
    vop_ref_inc(node), vop_open_inc(node);
    to->node = node;
    fd_array_open(to);
}

// fd2file - use fd as index of fd_array, return the array item (file)
/**
 * @brief 通过fd得到当前进程对应文件打开表的file文件
 * 
 * @param fd 想得到的文件的文件描述符
 * @param file_store 存储结果
 * @return int 成功返回0
 */
static inline int
fd2file(int fd, struct file **file_store) {
    if (testfd(fd)) {
        struct file *file = get_fd_array() + fd;
        if (file->status == FD_OPENED && file->fd == fd) {
            *file_store = file;
            return 0;
        }
    }
    return -E_INVAL;
}

// file_testfd - test file is readble or writable?
/**
 * @brief 检测fd对应文件的读写权限，和参数一样返回1,不一样或者没有fd返回0
 * 
 * @param fd 
 * @param readable 
 * @param writable 
 * @return true 
 * @return false 
 */
bool
file_testfd(int fd, bool readable, bool writable) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return 0;
    }
    if (readable && !file->readable) {
        return 0;
    }
    if (writable && !file->writable) {
        return 0;
    }
    return 1;
}


// open file
/**
 * @brief 被sysfile_open調用
 *        通用文件系统接口
 *        打开path目录下的文件
 * @param path 打开文件目录
 * @param open_flags 读写方式
 * @return int 返回fd
 */
int
file_open(char *path, uint32_t open_flags) {
    bool readable = 0, writable = 0;
    switch (open_flags & O_ACCMODE) { //0_ACCMODE是掩码，取后三位
    /* 读写模式:只读，只写，读写 */
    case O_RDONLY: readable = 1; break;
    case O_WRONLY: writable = 1; break;
    case O_RDWR:
        readable = writable = 1;
        break;
    default:
        return -E_INVAL;
    }
 
    int ret;
    struct file *file;
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0) {
        return ret;
    }

    struct inode *node;
    if ((ret = vfs_open(path, open_flags, &node)) != 0) {
        fd_array_free(file);
        return ret;
    }

    file->pos = 0;
    if (open_flags & O_APPEND) { /* 取openflag第二位即打开方式为读写 */
        struct stat __stat, *stat = &__stat;
        if ((ret = vop_fstat(node, stat)) != 0) {
            vfs_close(node);
            fd_array_free(file);
            return ret;
        }
        file->pos = stat->st_size;
    }

    file->node = node;
    file->readable = readable;
    file->writable = writable;
    fd_array_open(file);
    return file->fd;
}

// close file
/**
 * @brief 被sysfile_close調用
 *        通用文件接口层关闭文件
 * @param fd 关闭文件的文件描述符
 * @return int 成功返回0
 */
int
file_close(int fd) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_close(file);
    return 0;
}

// read file
/**
 * @brief 通用文件接口读文件
 * 
 * @param fd 
 * @param base 
 * @param len 数据长度最大IOBUF_SIZE
 * @param copied_store 长度的地址
 * @return int 
 */
int
file_read(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->readable) {
        return -E_INVAL;
    }
    fd_array_acquire(file); 

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos); /* 初始化iob */
    ret = vop_read(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
    return ret;
}

// write file

/**
 * @brief 通用文件接口写文件，下层调用vop_write接口
 * 
 * @param fd 
 * @param base 
 * @param len 数据长度
 * @param copied_store 长度的地址
 * @return int
 */
int
file_write(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->writable) {
        return -E_INVAL;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_write(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
    return ret;
}

// seek file
int
file_seek(int fd, off_t pos, int whence) {
    struct stat __stat, *stat = &__stat;
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);

    switch (whence) {
    case LSEEK_SET: break;
    case LSEEK_CUR: pos += file->pos; break;
    case LSEEK_END:
        if ((ret = vop_fstat(file->node, stat)) == 0) {
            pos += stat->st_size;
        }
        break;
    default: ret = -E_INVAL;
    }

    if (ret == 0) {
        if ((ret = vop_tryseek(file->node, pos)) == 0) {
            file->pos = pos;
        }
//    cprintf("file_seek, pos=%d, whence=%d, ret=%d\n", pos, whence, ret);
    }
    fd_array_release(file);
    return ret;
}

// stat file
int
file_fstat(int fd, struct stat *stat) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);
    ret = vop_fstat(file->node, stat);
    fd_array_release(file);
    return ret;
}

// sync file
int
file_fsync(int fd) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);
    ret = vop_fsync(file->node);
    fd_array_release(file);
    return ret;
}

// get file entry in DIR
int
file_getdirentry(int fd, struct dirent *direntp) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, direntp->name, sizeof(direntp->name), direntp->offset);
    if ((ret = vop_getdirentry(file->node, iob)) == 0) {
        direntp->offset += iobuf_used(iob);
    }
    fd_array_release(file);
    return ret;
}

// duplicate file
int
file_dup(int fd1, int fd2) {
    int ret;
    struct file *file1, *file2;
    if ((ret = fd2file(fd1, &file1)) != 0) {
        return ret;
    }
    if ((ret = fd_array_alloc(fd2, &file2)) != 0) {
        return ret;
    }
    fd_array_dup(file2, file1);
    return file2->fd;
}


