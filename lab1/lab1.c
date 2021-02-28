// Заголовочные файлы ядра
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

// Метаданные модуля ядра
MODULE_LICENSE("MIT");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("lab1");
MODULE_VERSION("0.1");

static int __init lab1_init(void) {
	printk(KERN_INFO "Hello, World!\n");
	return 0;
}

static void __exit lab1_exit(void) {
	printk(KERN_INFO "Goodbye, World!\n");
}

module_init(lab1_init);
module_exit(lab1_exit);
