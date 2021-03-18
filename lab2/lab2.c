#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("lab2");
MODULE_VERSION("0.1");

static int __init lab2_init(void) { return 0; }
static void __exit lab2_exit(void) {}

module_init(lab2_init);
module_exit(lab2_exit);
