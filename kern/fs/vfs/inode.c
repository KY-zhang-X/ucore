#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <atomic.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>
#include <assert.h>
#include <kmalloc.h>

/* *
 * __alloc_inode - alloc a inode structure and initialize in_type
 * */

/**
 * @brief 分配一个inode并且初始化in_type
 * 
 * @param type in_type类型
 * @return struct inode* 
 */
struct inode *
__alloc_inode(int type) {
    struct inode *node;
    if ((node = kmalloc(sizeof(struct inode))) != NULL) {
        node->in_type = type;
    }
    return node;
}

/* *
 * inode_init - initialize a inode structure
 * invoked by vop_init
 * */

/**
 * @brief 初始inode 被vop_init()调用
 * 
 * @param node 
 * @param ops 
 * @param fs 
 */
void
inode_init(struct inode *node, const struct inode_ops *ops, struct fs *fs) {
    node->ref_count = 0;
    node->open_count = 0;
    node->in_ops = ops, node->in_fs = fs;
    vop_ref_inc(node);
}

/* *
 * inode_kill - kill a inode structure
 * invoked by vop_kill
 * */

/**
 * @brief 释放一个inode内存空间
 * 
 * @param node inode地址
 */
void
inode_kill(struct inode *node) {
    assert(inode_ref_count(node) == 0);
    assert(inode_open_count(node) == 0);
    kfree(node);
}

/* *
 * inode_ref_inc - increment ref_count
 * invoked by vop_ref_inc
 * */

/**
 * @brief inode引用计数ref_count+1，vop_ref_inc宏调用
 * 
 * @param node 
 * @return int 返回当前inode引用
 */
int
inode_ref_inc(struct inode *node) {
    node->ref_count += 1;
    return node->ref_count;
}

/* *
 * inode_ref_dec - decrement ref_count
 * invoked by vop_ref_dec
 * calls vop_reclaim if the ref_count hits zero
 * */

/**
 * @brief inode引用计数ref_count-1，vop_ref_dec宏调用
 *        如果ref_count减到0调用vop_reclaim
 * @param node 
 * @return int 
 */
int
inode_ref_dec(struct inode *node) {
    assert(inode_ref_count(node) > 0);
    int ref_count, ret;
    node->ref_count-= 1;
    ref_count = node->ref_count;
    if (ref_count == 0) {
        if ((ret = vop_reclaim(node)) != 0 && ret != -E_BUSY) {
            cprintf("vfs: warning: vop_reclaim: %e.\n", ret);
        }
    }
    return ref_count;
}

/* *
 * inode_open_inc - increment the open_count
 * invoked by vop_open_inc
 * */

/**
 * @brief inode文件打开计数open_count++
 * 
 * @param node 
 * @return int 
 */
int
inode_open_inc(struct inode *node) {
    node->open_count += 1;
    return node->open_count;
}

/* *
 * inode_open_dec - decrement the open_count
 * invoked by vop_open_dec
 * calls vop_close if the open_count hits zero
 * */

/**
 * @brief 文件打开计数open_count--  
 *        如果打开计数为0,调用vop_close
 * @param node 
 * @return int 
 */
int
inode_open_dec(struct inode *node) {
    assert(inode_open_count(node) > 0);
    int open_count, ret;
    node->open_count -= 1;
    open_count = node->open_count;
    if (open_count == 0) {
        if ((ret = vop_close(node)) != 0) {
            cprintf("vfs: warning: vop_close: %e.\n", ret);
        }
    }
    return open_count;
}

/* *
 * inode_check - check the various things being valid
 * called before all vop_* calls
 * */

/**
 * @brief 调用vop_foo操作前进行检查，判断一致性
 * 
 * @param node 
 * @param opstr 
 */
void
inode_check(struct inode *node, const char *opstr) {
    assert(node != NULL && node->in_ops != NULL);
    assert(node->in_ops->vop_magic == VOP_MAGIC);
    int ref_count = inode_ref_count(node), open_count = inode_open_count(node);
    assert(ref_count >= open_count && open_count >= 0);
    assert(ref_count < MAX_INODE_COUNT && open_count < MAX_INODE_COUNT);
}

