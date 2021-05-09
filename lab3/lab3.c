#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/udp.h>
#include <linux/version.h>
#include <net/arp.h>

static char *link = "enp0s3";
module_param(link, charp, 0);

static char *ifname = "vni%d";
static unsigned char data[1500];

static struct net_device_stats stats;

static struct net_device *child = NULL;
struct priv {
  struct net_device *parent;
};

static char check_frame(struct sk_buff *skb, unsigned char data_shift) {
  unsigned char *user_data_ptr = NULL;
  struct iphdr *ip = (struct iphdr *)skb_network_header(skb);
  struct udphdr *udp = NULL;
  int data_len = 0;

  if (IPPROTO_UDP == ip->protocol) {
    udp = (struct udphdr *)((unsigned char *)ip + (ip->ihl * 4));
    data_len = ntohs(udp->len) - sizeof(struct udphdr);
    user_data_ptr = (unsigned char *)(skb->data + sizeof(struct iphdr) +
                                      sizeof(struct udphdr)) +
                    data_shift;
    memcpy(data, user_data_ptr, data_len);
    data[data_len] = '\0';

    printk("Captured UDP datagram, saddr: %d.%d.%d.%d\n",
           ntohl(ip->saddr) >> 24, (ntohl(ip->saddr) >> 16) & 0x00FF,
           (ntohl(ip->saddr) >> 8) & 0x0000FF, (ntohl(ip->saddr)) & 0x000000FF);
    printk("daddr: %d.%d.%d.%d\n", ntohl(ip->daddr) >> 24,
           (ntohl(ip->daddr) >> 16) & 0x00FF,
           (ntohl(ip->daddr) >> 8) & 0x0000FF, (ntohl(ip->daddr)) & 0x000000FF);

    printk(KERN_INFO "Data length: %d. Data:", data_len);
    printk("%s", data);
    return 1;
  }
  return 0;
}

static rx_handler_result_t handle_frame(struct sk_buff **pskb) {
  // if (child) {

  if (check_frame(*pskb, 0)) {
    stats.rx_packets++;
    stats.rx_bytes += (*pskb)->len;
  }
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

  if (check_frame(skb, 14)) {
    stats.tx_packets++;
    stats.tx_bytes += skb->len;
  }

  if (priv->parent) {
    skb->dev = priv->parent;
    skb->priority = 1;
    dev_queue_xmit(skb);
    return 0;
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

int __init lab3_init(void) {
  int err = 0;
  struct priv *priv;
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
  printk(KERN_INFO "Module %s loaded", THIS_MODULE->name);
  printk(KERN_INFO "%s: create link %s", THIS_MODULE->name, child->name);
  printk(KERN_INFO "%s: registered rx handler for %s", THIS_MODULE->name,
         priv->parent->name);
  return 0;
}

void __exit lab3_exit(void) {
  struct priv *priv = netdev_priv(child);
  if (priv->parent) {
    rtnl_lock();
    netdev_rx_handler_unregister(priv->parent);
    rtnl_unlock();
    printk(KERN_INFO "%s: unregister rx handler for %s", THIS_MODULE->name,
           priv->parent->name);
  }
  unregister_netdev(child);
  free_netdev(child);
  printk(KERN_INFO "Module %s unloaded", THIS_MODULE->name);
}

module_init(lab3_init);
module_exit(lab3_exit);

MODULE_AUTHOR("Author");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Description");
