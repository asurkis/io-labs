#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

// #define SECTOR_SIZE 512
#define MBR_SIZE SECTOR_SIZE
#define MBR_DISK_SIGNATURE_OFFSET 440
#define MBR_DISK_SIGNATURE_SIZE 4
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_ENTRY_SIZE 16
#define PARTITION_TABLE_SIZE 64
#define MBR_SIGNATURE_OFFSET 510
#define MBR_SIGNATURE_SIZE 2
#define MBR_SIGNATURE 0xAA55
#define BR_SIZE SECTOR_SIZE
#define BR_SIGNATURE_OFFSET 510
#define BR_SIGNATURE_SIZE 2
#define BR_SIGNATURE 0xAA55

typedef struct {
  unsigned char boot_type; // 0x00 - Inactive; 0x80 - Active (Bootable)
  unsigned char start_head;
  unsigned char start_sec : 6;
  unsigned char start_cyl_hi : 2;
  unsigned char start_cyl;
  unsigned char part_type;
  unsigned char end_head;
  unsigned char end_sec : 6;
  unsigned char end_cyl_hi : 2;
  unsigned char end_cyl;
  unsigned int abs_start_sec;
  unsigned int sec_in_part;
} PartEntry;

typedef PartEntry PartTable[4];

#define SEC_PER_HEAD 63
#define HEAD_PER_CYL 255
#define HEAD_SIZE (SEC_PER_HEAD * SECTOR_SIZE)
#define CYL_SIZE (SEC_PER_HEAD * HEAD_PER_CYL * SECTOR_SIZE)

#define sec4size(s) ((((s) % CYL_SIZE) % HEAD_SIZE) / SECTOR_SIZE)
#define head4size(s) (((s) % CYL_SIZE) / HEAD_SIZE)
#define cyl4size(s) ((s) / CYL_SIZE)

#define PART1_SIZE (10 * 2 * 1024)
#define PART2_SIZE (25 * 2 * 1024)
#define PART3_SIZE (15 * 2 * 1024)
// #define PART31_SIZE 0x5000
// #define PART32_SIZE 0x5000

#define MEMSIZE                                                                \
  (PART1_SIZE + PART2_SIZE + PART3_SIZE + 1) // Size of Ram disk in sectors
static int my_major_number = 0;              // Variable for Major Number

static PartTable def_part_table = {
    {
      boot_type : 0x00,
      start_sec : 0x1,
      start_head : 0x0,
      start_cyl_hi : 0x0,
      start_cyl : 0x0,
      part_type : 0x83,
      end_head : head4size(PART1_SIZE - 1),
      end_sec : sec4size(PART1_SIZE - 1) + 1,
      end_cyl_hi : (cyl4size(PART1_SIZE - 1) >> 8) & 0x3,
      end_cyl : cyl4size(PART1_SIZE - 1) & 0xFF,
      abs_start_sec : 0x1,
      sec_in_part : PART1_SIZE
    },
    {
      boot_type : 0x00,
      start_head : head4size(PART1_SIZE),
      start_sec : sec4size(PART1_SIZE) + 1,
      start_cyl_hi : (cyl4size(PART1_SIZE) >> 8) & 0x3,
      start_cyl : cyl4size(PART1_SIZE) & 0xFF,
      part_type : 0x83,
      end_sec : sec4size(PART1_SIZE + PART2_SIZE - 1) + 1,
      end_head : head4size(PART1_SIZE + PART2_SIZE - 1),
      end_cyl_hi : (cyl4size(PART1_SIZE + PART2_SIZE - 1) >> 8) & 0x3,
      end_cyl : cyl4size(PART1_SIZE + PART2_SIZE - 1) & 0xFF,
      abs_start_sec : PART1_SIZE + 1,
      sec_in_part : PART2_SIZE
    },
    {
      boot_type : 0x00,
      start_head : head4size(PART1_SIZE + PART2_SIZE),
      start_sec : sec4size(PART1_SIZE + PART2_SIZE) + 1,
      start_cyl_hi : (cyl4size(PART1_SIZE + PART2_SIZE) >> 8) & 0x3,
      start_cyl : cyl4size(PART1_SIZE + PART2_SIZE) & 0xFF,
      part_type : 0x83,
      end_sec : sec4size(PART1_SIZE + PART2_SIZE + PART3_SIZE - 1) + 1,
      end_head : head4size(PART1_SIZE + PART2_SIZE + PART3_SIZE - 1),
      end_cyl_hi :
              (cyl4size(PART1_SIZE + PART2_SIZE + PART3_SIZE - 1) >> 8) & 0x3,
      end_cyl : cyl4size(PART1_SIZE + PART2_SIZE + PART3_SIZE - 1) & 0xFF,
      abs_start_sec : PART1_SIZE + PART2_SIZE + 1,
      sec_in_part : PART3_SIZE + 2
    }};

static unsigned int def_log_part_br_abs_start_sector[] = {
    // PART1_SIZE + PART2_SIZE + 1,
    // PART1_SIZE + PART2_SIZE + PART31_SIZE + 2,
};
static const PartTable def_log_part_table[] = {
    // {{
    //    boot_type : 0x00,
    //    start_head : 0,
    //    start_sec : 1,
    //    start_cyl_hi : 0,
    //    start_cyl : 0,
    //    part_type : 0x83,
    //    end_sec : sec4size(PART31_SIZE - 1) + 1,
    //    end_head : head4size(PART31_SIZE - 1),
    //    end_cyl_hi : (cyl4size(PART31_SIZE - 1) >> 8) & 0x3,
    //    end_cyl : cyl4size(PART31_SIZE - 1) & 0xFF,
    //    abs_start_sec : 0x1,
    //    sec_in_part : PART31_SIZE
    //  },
    //  {
    //    boot_type : 0x00,
    //    start_head : sec4size(PART31_SIZE) + 1,
    //    start_sec : head4size(PART31_SIZE),
    //    start_cyl_hi : (cyl4size(PART31_SIZE) >> 8) & 0x3,
    //    start_cyl : cyl4size(PART31_SIZE) & 0xFF,
    //    part_type : 0x05,
    //    end_sec : sec4size(PART31_SIZE + PART32_SIZE - 1) + 1,
    //    end_head : head4size(PART31_SIZE + PART32_SIZE - 1),
    //    end_cyl_hi : (cyl4size(PART31_SIZE + PART32_SIZE - 1) >> 8) & 0x3,
    //    end_cyl : cyl4size(PART31_SIZE + PART32_SIZE - 1) & 0xFF,
    //    abs_start_sec : PART31_SIZE + 1,
    //    sec_in_part : PART32_SIZE
    //  }},
    // {{
    //   boot_type : 0x00,
    //   start_head : 0,
    //   start_sec : 0,
    //   start_cyl_hi : 0,
    //   start_cyl : 0,
    //   part_type : 0x83,
    //   end_sec : 0,
    //   end_head : 0,
    //   end_cyl_hi : 0,
    //   end_cyl : 0,
    //   abs_start_sec : 0x1,
    //   sec_in_part : PART32_SIZE
    // }}
};

static void copy_mbr(u8 *disk) {
  memset(disk, 0x0, MBR_SIZE);
  *(unsigned long *)(disk + MBR_DISK_SIGNATURE_OFFSET) = 0x36E5756D;
  memcpy(disk + PARTITION_TABLE_OFFSET, &def_part_table, PARTITION_TABLE_SIZE);
  *(unsigned short *)(disk + MBR_SIGNATURE_OFFSET) = MBR_SIGNATURE;
}

static void copy_br(u8 *disk, int abs_start_sector,
                    const PartTable *part_table) {
  disk += abs_start_sector * SECTOR_SIZE;
  memset(disk, 0x0, BR_SIZE);
  memcpy(disk + PARTITION_TABLE_OFFSET, part_table, PARTITION_TABLE_SIZE);
  *(unsigned short *)(disk + BR_SIGNATURE_OFFSET) = BR_SIGNATURE;
}

void copy_mbr_n_br(u8 *disk) {
  int i;
  copy_mbr(disk);
  for (i = 0; i < ARRAY_SIZE(def_log_part_table); i++) {
    copy_br(disk, def_log_part_br_abs_start_sector[i], &def_log_part_table[i]);
  }
}

/* Structure associated with Block device*/
struct mydiskdrive_dev {
  int size;
  u8 *data;
  spinlock_t lock;
  struct request_queue *queue;
  struct gendisk *gd;
} device;

static int my_open(struct block_device *x, fmode_t mode) {
  int ret = 0;
  printk(KERN_INFO "mydiskdrive : open \n");
  return ret;
}

static void my_release(struct gendisk *disk, fmode_t mode) {
  printk(KERN_INFO "mydiskdrive : closed \n");
}

static struct block_device_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
};

int mydisk_init(void) {
  device.data = vmalloc(MEMSIZE * SECTOR_SIZE);
  /* Setup its partition table */
  copy_mbr_n_br(device.data);
  return MEMSIZE;
}

static int rb_transfer(struct request *req) {
  int dir = rq_data_dir(req);
  int ret = 0;
  /*starting sector where to do operation*/
  sector_t start_sector = blk_rq_pos(req);
  /* no of sector on which opn to be done*/
  unsigned int sector_cnt = blk_rq_sectors(req);
  struct bio_vec bv;
  struct req_iterator iter;
  sector_t sector_offset;
  unsigned int sectors;
  u8 *buffer;
  sector_offset = 0;
  rq_for_each_segment(bv, req, iter) {
    void *disk_data_ptr =
        device.data + ((start_sector + sector_offset) * SECTOR_SIZE);
    buffer = page_address(bv.bv_page) + bv.bv_offset;
    if (bv.bv_len % (SECTOR_SIZE) != 0) {
      printk(KERN_ERR "bio size is not a multiple ofsector size\n");
      ret = -EIO;
    }
    sectors = bv.bv_len / SECTOR_SIZE;
    printk(KERN_DEBUG "my disk: Start Sector: %llu, Sector Offset: %llu;\n"
                      "Buffer: %p; Length: %u sectors\n",
           (unsigned long long)start_sector, (unsigned long long)sector_offset,
           buffer, sectors);

    if (dir == WRITE) { /* Write to the device */
      memcpy(disk_data_ptr, buffer, sectors * SECTOR_SIZE);
    } else { /* Read from the device */
      memcpy(buffer, disk_data_ptr, sectors * SECTOR_SIZE);
    }
    sector_offset += sectors;
  }

  if (sector_offset != sector_cnt) {
    printk(KERN_ERR "mydisk: bio info doesn't match with the request info");
    ret = -EIO;
  }
  return ret;
}

/** request handling function**/
static void dev_request(struct request_queue *q) {
  struct request *req;
  int error;
  /*check active request for data transfer*/
  while ((req = blk_fetch_request(q)) != NULL) {
    error = rb_transfer(req);          // transfer the request for operation
    __blk_end_request_all(req, error); // end the request
  }
}

void device_setup(void) {
  mydisk_init();
  my_major_number =
      register_blkdev(my_major_number, "mydisk"); // major no. allocation
  printk(KERN_ALERT "Major Number is : %d", my_major_number);
  spin_lock_init(&device.lock); // lock for queue
  device.queue = blk_init_queue(dev_request, &device.lock);

  device.gd = alloc_disk(8);          // gendisk allocation
  device.gd->major = my_major_number; // major no to gendisk
  device.gd->first_minor = 0;         // first minor of gendisk

  device.gd->fops = &fops;
  device.gd->private_data = &device;
  device.gd->queue = device.queue;
  device.size = mydisk_init();
  printk(KERN_INFO "THIS IS DEVICE SIZE %d", device.size);
  sprintf(device.gd->disk_name, "mydisk");
  set_capacity(device.gd, device.size);
  add_disk(device.gd);
}

static int __init mydiskdrive_init(void) {
  device_setup();
  return 0;
}

void mydisk_cleanup(void) { vfree(device.data); }

void __exit mydiskdrive_exit(void) {
  del_gendisk(device.gd);
  put_disk(device.gd);
  blk_cleanup_queue(device.queue);
  unregister_blkdev(my_major_number, "mydisk");
  mydisk_cleanup();
}

module_init(mydiskdrive_init);
module_exit(mydiskdrive_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Surkis");
MODULE_DESCRIPTION("Lab2");
