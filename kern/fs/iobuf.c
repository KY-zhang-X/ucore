#include <defs.h>
#include <string.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

/* 
 * iobuf_init - init io buffer struct.
 *                set up io_base to point to the buffer you want to transfer to, and set io_len to the length of buffer;
 *                initialize io_offset as desired;
 *                initialize io_resid to the total amount of data that can be transferred through this io.
 */

/**
 * @brief 
 * 
 * @param iob 
 * @param base 
 * @param len 
 * @param offset 
 * @return struct iobuf* 
 */
struct iobuf *
iobuf_init(struct iobuf *iob, void *base, size_t len, off_t offset) {
    iob->io_base = base;
    iob->io_offset = offset;
    iob->io_len = iob->io_resid = len;
    return iob;
}

/* iobuf_move - move data  (iob->io_base ---> data OR  data --> iob->io.base) in memory
 * @copiedp:  the size of data memcopied
 *
 * iobuf_move may be called repeatedly on the same io to transfer
 * additional data until the available buffer space the io refers to
 * is exhausted.
 */

/**
 * @brief iobuf_move - 移动内存数据 将数据从iob->io_base拷贝到data 或者 从data拷贝到iobuf
 * 
 * @param iob 
 * @param data 
 * @param len 
 * @param m2b 1表示从data拷贝到iob 0表示从iob拷贝到data
 * @param copiedp 用来接收拷贝长度的地址
 * @return int 
 */
int
iobuf_move(struct iobuf *iob, void *data, size_t len, bool m2b, size_t *copiedp) {
    size_t alen;
    if ((alen = iob->io_resid) > len) { /* 比较拷贝长度和当前iob剩余容量，决定真实拷贝长度 */
        alen = len;
    }
    if (alen > 0) {
        void *src = iob->io_base, *dst = data;
        if (m2b) {
            void *tmp = src;
            src = dst, dst = tmp;
        }
        memmove(dst, src, alen);
        iobuf_skip(iob, alen), len -= alen;
    }
    if (copiedp != NULL) {
        *copiedp = alen;
    }
    return (len == 0) ? 0 : -E_NO_MEM;
}

/*
 * iobuf_move_zeros - set io buffer zero
 * @copiedp:  the size of data memcopied
 */

/**
 * @brief 向当前iob写入len个0，不够就写满
 * 
 * @param iob 
 * @param len 打算写入0的长度
 * @param copiedp 接受拷贝长度的地址
 * @return int 
 */
int
iobuf_move_zeros(struct iobuf *iob, size_t len, size_t *copiedp) {
    size_t alen;
    if ((alen = iob->io_resid) > len) {
        alen = len;
    }
    if (alen > 0) {
        memset(iob->io_base, 0, alen);
        iobuf_skip(iob, alen), len -= alen;
    }
    if (copiedp != NULL) {
        *copiedp = alen;
    }
    return (len == 0) ? 0 : -E_NO_MEM;
}

/*
 * iobuf_skip - change the current position of io buffer
 */

/**
 * @brief 更新iob中的io_offset和io_resid
 * 
 * @param iob 
 * @param n 
 */
void
iobuf_skip(struct iobuf *iob, size_t n) {
    assert(iob->io_resid >= n);
    iob->io_base += n, iob->io_offset += n, iob->io_resid -= n;
}

