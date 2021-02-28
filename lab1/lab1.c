// Заголовочные файлы ядра
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

// Метаданные модуля ядра
MODULE_LICENSE("MIT");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("lab1");
MODULE_VERSION("0.1");

static ssize_t lab1_read(struct file *f, char __user *ubuf, size_t count,
                         loff_t *ppos) {
  printk(KERN_INFO, "Read handler!\n");
  return 0;
}

static ssize_t lab1_write(struct file *f, const char __user *ubuf, size_t count,
                         loff_t *ppos) {
  printk(KERN_INFO, "Write handler!\n");
  return -1;
}

static struct file_operations my_proc_fops = {
    .owner = THIS_MODULE, .read = lab1_read, .write = lab1_write};

static struct proc_dir_entry *my_proc_file = NULL;

static int __init lab1_init(void) {
  my_proc_file = proc_create("var2", 0444, NULL, &my_proc_fops);
  printk(KERN_INFO "Hello, World!\n");
  return 0;
}

static void __exit lab1_exit(void) {
  proc_remove(my_proc_file);
  printk(KERN_INFO "Goodbye, World!\n");
}

module_init(lab1_init);
module_exit(lab1_exit);
