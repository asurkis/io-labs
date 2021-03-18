#include <linux/fs.h>
#include <linux/module.h>
#include <linux/genhd.h>

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

static int init_stage;
static int MY_BLOCK_MAJOR;

static void lab2_exit(void) {
  switch (init_stage) {
  case 2:
  case 1:
    unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
  case 0:
    break;
  }
  init_stage = 0;
}

#define CHKERR_INIT(action)                                                    \
  do {                                                                         \
    int __status_##__LINE__ = action;                                          \
    if (__status_##__LINE__ < 0) {                                             \
      lab2_exit();                                                             \
      return __status_##__LINE__;                                              \
    }                                                                          \
    ++init_stage;                                                              \
  } while (0);

static int lab2_init(void) {
  init_stage = 0;
  MY_BLOCK_MAJOR = register_blkdev(0, MY_BLKDEV_NAME);
  CHKERR_INIT(MY_BLOCK_MAJOR);
  return 0;
}

module_init(lab2_init);
module_exit(lab2_exit);
