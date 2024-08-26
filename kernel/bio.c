// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


// #include "types.h"
// #include "param.h"
// #include "spinlock.h"
// #include "sleeplock.h"
// #include "riscv.h"
// #include "defs.h"
// #include "fs.h"
// #include "buf.h"

// #define BUCKET_NUM 13

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];//桶链表
//   struct buf *buckets[BUCKET_NUM];  //每个桶有一个链表头
//   struct spinlock bucket_locks[BUCKET_NUM]; // 每个桶一个锁
// } bcache;

// static int
// hash(uint dev, uint blockno)
// {
//   //获取哈希桶的编号
//   return (dev ^ blockno) % BUCKET_NUM;
// }

// void
// binit(void)
// {
//   printf("调用binit\n");////////////////////////////////////
//   initlock(&bcache.lock, "bcache");//初始化大锁
//   for(int i = 0; i < BUCKET_NUM; i++) {
//     initlock(&bcache.bucket_locks[i], "bcache.bucket");
//     bcache.buckets[i] = 0;//////////////////////////疑问：是这样还是->next=0
//   }
//   struct buf *b;//初始化buf链表
//   for(b = bcache.buf; b < bcache.buf + NBUF; b++){
//     b->next = 0;
//     b->dev = 0;
//     b->blockno = 0;
//     b->valid = 0;
//     b->refcnt = 0;
//     b->lastuse = 0;
//     initsleeplock(&b->lock, "buffer");
//   }
//   printf("结束调用binit\n");////////////////////////////////////
// }


// static struct buf*
// bget(uint dev, uint blockno)
// {
//   printf("调用bget\n");////////////////////////////////////
//   struct buf *b;
//   int index = hash(dev, blockno);

//   acquire(&bcache.bucket_locks[index]);//获取桶的锁
//   // 查找块是否已经在缓存中（链表形式遍历）
//   for(b = bcache.buckets[index]; b; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       release(&bcache.bucket_locks[index]);//释放桶的锁
//       acquiresleep(&b->lock);  // 锁住单个 buf
//       b->refcnt++;
//       b->lastuse = ticks;
//       return b;
//     }
//   }

//   // 如果未找到，则需要分配或替换一个缓冲区（数组形式遍历）
//   struct buf *victim = 0;
//   for(b = bcache.buf; b < bcache.buf + NBUF; b++){
//     if(b->refcnt == 0 && (!victim || b->lastuse < victim->lastuse)){
//       victim = b;
//     }
//   }
//   if(victim == 0){
//     release(&bcache.bucket_locks[index]);//释放桶的锁
//     panic("bget: no buffers");
//   }
//   // 从旧桶中移除（将它从旧桶链表中摘下）
//   int old_h = hash(victim->dev, victim->blockno);
//   if(old_h!=index)
//     acquire(&bcache.bucket_locks[old_h]);///////获取旧桶锁
//   if(victim->next)
//     victim->next->prev = victim->prev;
//   if(victim->prev)
//     victim->prev->next = victim->next;
//   if(bcache.buckets[old_h] == victim)//若摘下的是头指针
//     bcache.buckets[old_h] = victim->next;
//   if(old_h!=index)
//     release(&bcache.bucket_locks[old_h]); // 释放旧桶的锁
//   // 更新块的信息并将其插入新桶
//   victim->dev = dev;
//   victim->blockno = blockno;
//   victim->refcnt = 1;
//   victim->valid = 0;
//   victim->lastuse = ticks;//////////////////////////////锁的问题
//   //头插法
//   victim->next = bcache.buckets[index];
//   bcache.buckets[index] = victim; 

//   release(&bcache.bucket_locks[index]);
//   acquiresleep(&victim->lock); 
//   printf("结束调用bget\n");////////////////////////////////////
//   return victim;
// }

// void
// brelse(struct buf *b)
// {
//   printf("调用brelse\n");////////////////////////////////////
//   b->lastuse = ticks;
//   b->refcnt--;
//   releasesleep(&b->lock);

//   int h = hash(b->dev, b->blockno);
//   acquire(&bcache.bucket_locks[h]);
//   if(b->refcnt == 0){
//     // 如果没有被引用，将其放回到桶的链表中
//     b->next = bcache.buckets[h];
//     bcache.buckets[h] = b;
//   }
//   release(&bcache.bucket_locks[h]);
//   printf("结束调用brelse\n");////////////////////////////////////
// }


// // Return a locked buf with the contents of the indicated block.
// struct buf*
// bread(uint dev, uint blockno)
// {
//   struct buf *b;

//   b = bget(dev, blockno);//获取指定设备和块号的缓冲区
//   if(!b->valid) {
//     virtio_disk_rw(b, 0);//如果缓冲区内容无效，则从磁盘读取数据，并标记为有效
//     b->valid = 1;
//   }
//   return b;
// }

// // Write b's contents to disk.  Must be locked.
// void
// bwrite(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))//确保缓冲区已被锁定
//     panic("bwrite");
//   virtio_disk_rw(b, 1);//将缓冲区内容写回磁盘
// }


// void
// bpin(struct buf *b) {//增加缓冲区的引用计数
//   acquire(&bcache.lock);
//   b->refcnt++;
//   release(&bcache.lock);
// }

// void
// bunpin(struct buf *b) {//减少缓冲区的引用计数
//   acquire(&bcache.lock);
//   b->refcnt--;
//   release(&bcache.lock);
// }


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKET_NUM 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];//桶链表
  struct buf buckets[BUCKET_NUM];  //每个桶有一个链表头
  struct spinlock bucket_locks[BUCKET_NUM]; // 每个桶一个锁
} bcache;

static int
hash(uint dev, uint blockno)
{
  //获取哈希桶的编号
  return (dev ^ blockno) % BUCKET_NUM;
}

void
binit(void)
{
  initlock(&bcache.lock, "lock");//初始化大锁
  for(int i = 0; i < BUCKET_NUM; i++){
    initlock(&bcache.bucket_locks[i],"bcache_buckets");//初始化每个哈希桶的锁
    bcache.buf[i].next = 0;//初始化，头链表下一个为null
  }

  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");//初始化睡眠锁
    b->refcnt = 0;
    b->lastuse = 0;
    //下面尝试把buf全都链接到一个桶的链表里
    b->next = bcache.buckets[0].next;
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{

  uint index = hash(dev,blockno);
  acquire(&bcache.bucket_locks[index]);  

  struct buf *b;
  for(b = bcache.buckets[index].next;b!=0;b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      //命中的情况
      b->refcnt++;
      release(&bcache.bucket_locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[index]);

  acquire(&bcache.lock);//获取最大的锁
  //首先在大锁下再遍历一遍，防止遗漏
  for(b = bcache.buckets[index].next;b!=0;b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bucket_locks[index]);
      b->refcnt++;
      release(&bcache.bucket_locks[index]);
      release(&bcache.lock); 
      acquiresleep(&b->lock); 
      return b;
    }
  }

  struct buf * victim = 0;
  uint holding_bucket = -1;  
  for(int i = 0; i < BUCKET_NUM; i++){
    int found = 0;   
    acquire(&bcache.bucket_locks[i]);
    for(b = &bcache.buckets[i]; b->next; b = b->next){
      if(b->next->refcnt == 0 && (!victim || b->next->lastuse < victim->next->lastuse)){
        victim = b;
        found = 1;//找到可替换的内存块
      }
    }
    if(!found){  
      release(&bcache.bucket_locks[i]);
    }else{  
      if(holding_bucket != -1)
        release(&bcache.bucket_locks[holding_bucket]);
      holding_bucket = i;
    }
  }

  if(!victim){  
    panic("bget: no buffers!");
  }
  b = victim->next;
  if(holding_bucket !=index){ //被替换的块在别的桶链表中
    victim->next = b->next;//摘下来
    release(&bcache.bucket_locks[holding_bucket]);
    acquire(&bcache.bucket_locks[index]);
    
    b->next= bcache.buckets[index].next;
    bcache.buckets[index].next = b;//放到第一个位置
  }
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bucket_locks[index]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint index = hash(b->dev,b->blockno);

  acquire(&bcache.bucket_locks[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse = ticks;
  }

  release(&bcache.bucket_locks[index]);
}

void
bpin(struct buf *b) {
  uint index = hash(b->dev, b->blockno);

  acquire(&bcache.bucket_locks[index]);
  b->refcnt++;
  release(&bcache.bucket_locks[index]);
}

void
bunpin(struct buf *b) {
  uint index = hash(b->dev, b->blockno);

  acquire(&bcache.bucket_locks[index]);
  b->refcnt--;
  release(&bcache.bucket_locks[index]);
}