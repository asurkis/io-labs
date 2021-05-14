#include <linux/cdev.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/udp.h>
#include <linux/version.h>
#include <net/arp.h>

static char *link = "enp0s3";
module_param(link, charp, 0);

static char *ifname = "vni%d";

#define IPV4_STR_MAX_SIZE (sizeof("255.255.255.255"))
static char saddr_buffer[IPV4_STR_MAX_SIZE];
static char daddr_buffer[IPV4_STR_MAX_SIZE];

static char stat_buffer[4096];
static size_t stat_buffer_len = 0;
static int stat_buffer_outdated = 1;

static struct net_device_stats stats;

static struct net_device *child = NULL;
struct priv {
  struct net_device *parent;
};

static ssize_t lab1_dev_read(struct file *f, char __user *ubuf, size_t count,
                             loff_t *ppos) { 
  return -1; 
}

static char current_mode = '1';

static ssize_t lab1_dev_write(struct file *f, const char __user *ubuf,
                              size_t count, loff_t *ppos) {
  char c;
  if (!count) return 0;
  if (*ppos) return 0;

  c = ubuf[0];
  switch (c) {
  case '1':
  case '2':
    current_mode = c;
    break;
  default:
    return -1;
  }

  return count;
}

static struct file_operations lab1_dev_fops = {
    .owner = THIS_MODULE, .read = lab1_dev_read, .write = lab1_dev_write};

static void check_frame(struct sk_buff *skb, unsigned char data_shift) {
  struct iphdr *ip = (struct iphdr *)skb_network_header(skb);

  if (current_mode == '1') {
    unsigned char s1 = 255 & (ntohl(ip->saddr) >> 24);
    unsigned char s2 = 255 & (ntohl(ip->saddr) >> 16);
    unsigned char s3 = 255 & (ntohl(ip->saddr) >> 8);
    unsigned char s4 = 255 & (ntohl(ip->saddr));
    snprintf(saddr_buffer, IPV4_STR_MAX_SIZE, "%d.%d.%d.%d", s1, s2, s3, s4);
    printk(KERN_INFO "Captured IPv4 frame, saddr:%*s", (int) IPV4_STR_MAX_SIZE, saddr_buffer);
  } else {
    unsigned char d1 = 255 & (ntohl(ip->daddr) >> 24);
    unsigned char d2 = 255 & (ntohl(ip->daddr) >> 16);
    unsigned char d3 = 255 & (ntohl(ip->daddr) >> 8);
    unsigned char d4 = 255 & (ntohl(ip->daddr));
    snprintf(daddr_buffer, IPV4_STR_MAX_SIZE, "%d.%d.%d.%d", d1, d2, d3, d4);
    printk(KERN_INFO "Captured IPv4 frame, daddr:%*s", (int) IPV4_STR_MAX_SIZE, daddr_buffer);
  }
}

static rx_handler_result_t handle_frame(struct sk_buff **pskb) {
  // if (child) {

  check_frame(*pskb, 0);
  stats.rx_packets++;
  stats.rx_bytes += (*pskb)->len;
  stat_buffer_outdated = 1;
  (*pskb)->dev = child;
  return RX_HANDLER_ANOTHER;
  //}
  return RX_HANDLER_PASS;
}

static int open(struct net_device *dev) {
  netif_start_queue(dev);
  printk(KERN_INFO "%s: device opened", dev->name);
  return 0;
}

static int stop(struct net_device *dev) {
  netif_stop_queue(dev);
  printk(KERN_INFO "%s: device closed", dev->name);
  return 0;
}

static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev) {
  struct priv *priv = netdev_priv(dev);

  check_frame(skb, 14);
  stats.tx_packets++;
  stats.tx_bytes += skb->len;
  stat_buffer_outdated = 1;

  if (priv->parent) {
    skb->dev = priv->parent;
    skb->priority = 1;
    dev_queue_xmit(skb);
  }

  return NETDEV_TX_OK;
}

static struct net_device_stats *get_stats(struct net_device *dev) {
  return &stats;
}

static struct net_device_ops crypto_net_device_ops = {
    .ndo_open = open,
    .ndo_stop = stop,
    .ndo_get_stats = get_stats,
    .ndo_start_xmit = start_xmit};

static void setup(struct net_device *dev) {
  int i;
  ether_setup(dev);
  memset(netdev_priv(dev), 0, sizeof(struct priv));
  dev->netdev_ops = &crypto_net_device_ops;

  // fill in the MAC address with a phoney
  for (i = 0; i < ETH_ALEN; i++)
    dev->dev_addr[i] = (char)i;
}

static ssize_t proc_read(struct file *f, char __user *ubuf, size_t count,
                         loff_t *ppos) {
  size_t len;
  if (stat_buffer_outdated) {
    stat_buffer_len = snprintf(stat_buffer, sizeof(stat_buffer),
                               "rx_packets : %lu\n"
                               "rx_bytes   : %lu\n"
                               "tx_packets : %lu\n"
                               "tx_bytes   : %lu\n",
                               stats.rx_packets, stats.rx_bytes,
                               stats.tx_packets, stats.tx_bytes);
    stat_buffer_outdated = 0;
  }
  len = stat_buffer_len + 1;
  if (*ppos + len > sizeof(stat_buffer))
    len = sizeof(stat_buffer) - *ppos;
  if (len > count)
    len = count;
  if (copy_to_user(ubuf, stat_buffer + *ppos, len))
    return -EFAULT;
  *ppos += len;
  return len;
}

static ssize_t proc_write(struct file *f, const char __user *ubuf, size_t count,
                          loff_t *ppos) {
  return -1;
}

static struct file_operations proc_fops = {
    .owner = THIS_MODULE, .read = proc_read, .write = proc_write};

static struct proc_dir_entry *proc_file = NULL;

static dev_t dev;
static struct cdev lab1_cdev[1];
static struct class *c1;

static char *devnode(struct device *dev, umode_t *mode) {
  if (mode)
    *mode = 0666;
  return NULL;
}

int __init lab3_init(void) {
  struct priv *priv;
  int err = alloc_chrdev_region(&dev, 0, 1, "lab1_dev_driver");
  if (err < 0)
    return err;

  if ((c1 = class_create(THIS_MODULE, "lab1_dev_driver_class")) == NULL)
    return -1;

  c1->devnode = devnode;
  if (device_create(c1, NULL, dev, NULL, "var2") == NULL)
    return -1;

  cdev_init(&lab1_cdev[0], &lab1_dev_fops);
  if (cdev_add(&lab1_cdev[0], dev, 2) < 0)
    return -1;

  child = alloc_netdev(sizeof(struct priv), ifname, NET_NAME_UNKNOWN, setup);
  if (child == NULL) {
    printk(KERN_ERR "%s: allocate error", THIS_MODULE->name);
    return -ENOMEM;
  }
  priv = netdev_priv(child);
  priv->parent = __dev_get_by_name(&init_net, link); // parent interface
  if (!priv->parent) {
    printk(KERN_ERR "%s: no such net: %s", THIS_MODULE->name, link);
    free_netdev(child);
    return -ENODEV;
  }
  if (priv->parent->type != ARPHRD_ETHER &&
      priv->parent->type != ARPHRD_LOOPBACK) {
    printk(KERN_ERR "%s: illegal net type", THIS_MODULE->name);
    free_netdev(child);
    return -EINVAL;
  }

  // copy IP, MAC and other information
  memcpy(child->dev_addr, priv->parent->dev_addr, ETH_ALEN);
  memcpy(child->broadcast, priv->parent->broadcast, ETH_ALEN);
  if ((err = dev_alloc_name(child, child->name))) {
    printk(KERN_ERR "%s: allocate name, error %i", THIS_MODULE->name, err);
    free_netdev(child);
    return -EIO;
  }

  register_netdev(child);
  rtnl_lock();
  netdev_rx_handler_register(priv->parent, &handle_frame, NULL);
  rtnl_unlock();

  proc_file = proc_create("var2", 0444, NULL, &proc_fops);

  printk(KERN_INFO "Module %s loaded", THIS_MODULE->name);
  printk(KERN_INFO "%s: create link %s", THIS_MODULE->name, child->name);
  printk(KERN_INFO "%s: registered rx handler for %s", THIS_MODULE->name,
         priv->parent->name);
  return 0;
}

void __exit lab3_exit(void) {
  struct priv *priv = netdev_priv(child);

  if (proc_file)
    proc_remove(proc_file);

  if (priv->parent) {
    rtnl_lock();
    netdev_rx_handler_unregister(priv->parent);
    rtnl_unlock();
    printk(KERN_INFO "%s: unregister rx handler for %s", THIS_MODULE->name,
           priv->parent->name);
  }
  unregister_netdev(child);
  free_netdev(child);

  device_destroy(c1, dev);
  class_destroy(c1);
  unregister_chrdev_region(dev, 1);

  printk(KERN_INFO "Module %s unloaded", THIS_MODULE->name);
}

module_init(lab3_init);
module_exit(lab3_exit);

MODULE_AUTHOR("Anton Surkis");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("lab3");
