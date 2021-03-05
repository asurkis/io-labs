#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("lab1");
MODULE_VERSION("0.1");

#define VARIANT_NAME "var2"
#define RESULT_BUFLEN 4095
#define PRINT_BUFLEN 12

static char result_text[RESULT_BUFLEN + 1];
static size_t result_len = 0;

static void print_int(int value) {
  char print_buffer[PRINT_BUFLEN];
  size_t pos = PRINT_BUFLEN;
  int negative = value < 0 ? 1 : 0;
  value = value < 0 ? -value : value;
  print_buffer[--pos] = ' ';
  do {
    print_buffer[--pos] = '0' + value % 10;
    value /= 10;
  } while (value > 0);
  if (negative)
    print_buffer[--pos] = '-';
  if (result_len + PRINT_BUFLEN > pos + RESULT_BUFLEN)
    result_len = 0;
  while (pos < PRINT_BUFLEN)
    result_text[result_len++] = print_buffer[pos++];
}

static ssize_t lab1_proc_read(struct file *f, char __user *ubuf, size_t count,
                              loff_t *ppos) {
  size_t len = result_len - *ppos;
  if (len > count)
    len = count;
  if (copy_to_user(ubuf, result_text + *ppos, len))
    return -EFAULT;
  *ppos += len;
  return len;
}

static ssize_t lab1_proc_write(struct file *f, const char __user *ubuf,
                               size_t count, loff_t *ppos) {
  result_text[result_len] = 0;
  printk(KERN_INFO "%s\n", result_text);
  return -1;
}

static ssize_t lab1_dev_read(struct file *f, char __user *ubuf, size_t count,
                             loff_t *ppos) {
  result_text[result_len] = 0;
  printk(KERN_INFO "%s\n", result_text);
  return 0;
}

static ssize_t lab1_dev_write(struct file *f, const char __user *ubuf,
                              size_t count, loff_t *ppos) {
  int sa = 0, sb = 0;
  int a = 0, b = 0;
  int running;
  char op;
  size_t i = 0;

  if (*ppos)
    return 0;

  for (running = 1; running && i < count; ++i) {
    char c = ubuf[i];
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (!sa)
        sa = 1;
      a = 10 * a + c - '0';
      break;

    case '-':
      if (!sa) {
        sa = -1;
        break;
      }
    case '+':
    case '*':
    case '/':
      op = c;
      running = 0;
      break;
    }
  }

  for (running = 1; running && i < count; ++i) {
    char c = ubuf[i];
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (!sb)
        sb = 1;
      b = 10 * b + c - '0';
      break;

    case '-':
      if (!sb) {
        sb = -1;
        break;
      }
    }
  }

  a *= sa;
  b *= sb;
  switch (op) {
  case '+':
    print_int(a + b);
    break;
  case '-':
    print_int(a - b);
    break;
  case '*':
    print_int(a * b);
    break;
  case '/':
    if (b != 0)
      print_int(a / b);
    break;
  }

  return i;
}

static struct file_operations lab1_proc_fops = {
    .owner = THIS_MODULE, .read = lab1_proc_read, .write = lab1_proc_write};

static struct file_operations lab1_dev_fops = {
    .owner = THIS_MODULE, .read = lab1_dev_read, .write = lab1_dev_write};

static struct proc_dir_entry *lab1_proc_file = NULL;
/* #define DEV_MAJOR 137
#define DEV_MINOR 137
#define DEV MKDEV(DEV_MAJOR, DEV_MINOR) */
static dev_t dev;
static struct cdev lab1_cdev;
static struct class *c1;

static char *devnode(struct device *dev, umode_t *mode) {
  if (mode)
    *mode = 0666;
  return NULL;
}

static int __init lab1_init(void) {
  int err = alloc_chrdev_region(&dev, 0, 1, "lab1_dev_driver");
  if (err < 0)
    return err;
  if ((c1 = class_create(THIS_MODULE, "lab1_dev_driver_class")) == NULL) {
    unregister_chrdev_region(dev, 1);
    return -1;
  }
  c1->devnode = devnode;
  if (device_create(c1, NULL, dev, NULL, "var2") == NULL) {
    class_destroy(c1);
    unregister_chrdev_region(dev, 1);
    return -1;
  }
  cdev_init(&lab1_cdev, &lab1_dev_fops);
  if (cdev_add(&lab1_cdev, dev, 1) < 0) {
    device_destroy(c1, dev);
    class_destroy(c1);
    unregister_chrdev_region(dev, 1);
    return -1;
  }
  lab1_proc_file = proc_create("var2", 0444, NULL, &lab1_proc_fops);
  return 0;
}

static void __exit lab1_exit(void) {
  proc_remove(lab1_proc_file);
  cdev_del(&lab1_cdev);
  device_destroy(c1, dev);
  class_destroy(c1);
  unregister_chrdev_region(dev, 1);
}

module_init(lab1_init);
module_exit(lab1_exit);

