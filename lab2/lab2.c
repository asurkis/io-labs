#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("lab2");
MODULE_VERSION("0.1");

#define CHKERR(action)                                                         \
  do {                                                                         \
    int __status_##__LINE__ = action;                                          \
    if (__status_##__LINE__ < 0)                                               \
      return __status_##__LINE__;                                              \
  } while (0)

#define MY_BLKDEV_NAME "lab2dev"
#define MY_DISK_NAME "lab2_disk_name"
#define MY_SECTORS (5 * 1024 * 1024 / 512)
#define MY_KERNEL_SECTOR_SIZE 512

static int init_stage;

#define MY_BLOCK_MINORS 1

static struct my_block_dev {
  int block_major;
  u8 *data;
  struct gendisk *gd;
  spinlock_t lock;
  struct request_queue *queue;
} dev;

static int my_block_open(struct block_device *dev, fmode_t mode) { return 0; }

static int my_block_release(struct gendisk *gd, fmode_t mode) { return 0; }

static int my_block_request(struct request_queue *queue, spinlock_t *lock) { return 0; }

struct block_device_operations my_fops = {
    .owner = THIS_MODULE, .open = my_block_open, .release = my_block_release};

static int init_queue(struct my_block_dev *dev) {
  spin_lock_init(&dev->lock);
  dev->queue = blk_init_queue(my_block_request, &dev->lock);
  if (!dev->queue)
    return -1;
  blk_queue_logical_block_size(dev->queue, MY_KERNEL_SECTOR_SIZE);
  dev->queue->queuedata = dev;
  return 0;
}

static int create_block_device(struct my_block_dev *dev) {
  dev->gd = alloc_disk(MY_BLOCK_MINORS);
  printk(KERN_DEBUG "Allocated disk pointer: %p\n", dev->gd);
  dev->gd->major = dev->block_major;
  dev->gd->first_minor = 0;
  dev->gd->minors = MY_BLOCK_MINORS;
  dev->gd->fops = &my_fops;
  dev->gd->queue = dev->queue;
  dev->gd->private_data = dev;
  snprintf(dev->gd->disk_name, sizeof(dev->gd->disk_name), MY_DISK_NAME);
  set_capacity(dev->gd, MY_SECTORS);
  printk(KERN_DEBUG "Adding disk...\n");
  add_disk(dev->gd);
  printk(KERN_DEBUG "Added disk successfully\n");
  return 0;
}

static void delete_block_device(struct my_block_dev *dev) {
  del_gendisk(dev->gd);
}

static void lab2_exit(void);

#define CHKERR_INIT(action)                                                    \
  do {                                                                         \
    int __status_##__LINE__ = action;                                          \
    if (__status_##__LINE__ < 0) {                                             \
      return __status_##__LINE__;                                              \
    }                                                                          \
    ++init_stage;                                                              \
  } while (0);

static int lab2_init(void) {
  init_stage = 0;
  dev.block_major = register_blkdev(0, MY_BLKDEV_NAME);
  printk(KERN_DEBUG "Allocated major number %d\n", dev.block_major);
  CHKERR_INIT(dev.block_major);
  CHKERR_INIT(init_queue(&dev));
  CHKERR_INIT(create_block_device(&dev));
  return 0;
}

static void lab2_exit(void) {
  switch (init_stage) {
  case 3:
    printk(KERN_DEBUG "Deleting block device\n");
    delete_block_device(&dev);
  case 2:
    printk(KERN_DEBUG "Deleting queue\n");
    blk_cleanup_queue(dev.queue);
  case 1:
    printk(KERN_DEBUG "Unregistering major number\n");
    unregister_blkdev(dev.block_major, MY_BLKDEV_NAME);
  case 0:
    break;
  }
  init_stage = 0;
}

module_init(lab2_init);
module_exit(lab2_exit);
