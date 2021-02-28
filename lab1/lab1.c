#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("lab1");
MODULE_VERSION("0.1");

#define RESULT_BUFLEN 4096
#define PRINT_BUFLEN 12
static char result_text[RESULT_BUFLEN];
static size_t result_len = 0;

static void print_int(int value) {
  char print_buffer[PRINT_BUFLEN];
  size_t pos = PRINT_BUFLEN;
  int negative = value < 0 ? 1 : 0;
  printk(KERN_INFO "Printing number: %d", value);
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

static ssize_t lab1_proc_read(
    struct file *f,
    char __user *ubuf,
    size_t count,
    loff_t *ppos)
{
  size_t len = result_len - *ppos;
  if (len > count)
    len = count;
  if (copy_to_user(ubuf, result_text + *ppos, len))
    return -EFAULT;
  *ppos += len;
  return len;
}

static ssize_t lab1_proc_write(
    struct file *f,
    const char __user *ubuf,
    size_t count,
    loff_t *ppos)
{
  printk(KERN_INFO "Write handler!\n");
  return -1;
}

static struct file_operations my_proc_fops = {
    .owner = THIS_MODULE,
    .read = lab1_proc_read,
    .write = lab1_proc_write
};

static struct proc_dir_entry *my_proc_file = NULL;

static int __init lab1_init(void) {
  int i;
  for (i = 0; i < 5; ++i)
    print_int(i);
  my_proc_file = proc_create("var2", 0444, NULL, &my_proc_fops);
  return 0;
}

static void __exit lab1_exit(void) {
  proc_remove(my_proc_file);
}

module_init(lab1_init);
module_exit(lab1_exit);

