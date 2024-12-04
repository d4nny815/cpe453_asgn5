#ifndef MIN_COMMON_H
#define MIN_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/limits.h>
#include <string.h>

#define MIN_MINLS_ARGS  (2)

#define MIN_MINLS_ARGS  (2)
#define PART_TBL_OFFSET (0x1BE)
#define BOOT_BLOCK_SIZE (512)
#define SB_OFFSET       (1024)
#define VALID_BYTE510   (0x55)
#define VALID_BYTE511   (0xAA)
#define MINIX_MAGIC_NUM (0x4D5A)
#define MINIX_PART_TYPE (0x81)
#define INODE_SIZE      (64)
#define DIR_ENTRY_SIZE  (64)
#define SECTOR_SIZE     (512)
#define INVALID_INODE   (0)
#define ROOT_INODE      (1)
#define DEFAULT_PATH    ("/")

#define FILE_TYPE_MASK  (0xF000)
#define REGULAR_FILE    (0x8000)
#define DIRECTORY       (0x4000)
#define OWNER_READ      (0x100)
#define OWNER_WRITE     (0x80)
#define OWNER_EXEC      (0x40)
#define GROUP_READ      (0x20)
#define GROUP_WRITE     (0x10)
#define GROUP_EXEC      (8)
#define OTHER_READ      (4)
#define OTHER_WRITE     (2)
#define OTHER_EXEC      (1)

#define SAME_STR(x, y)  (strcmp(x, y) == 0)

typedef struct __attribute__ ((__packed__)) MinArgs_t {
    bool verbose;
    int partnum;
    int subnum;
    FILE* image_file;
    char path[PATH_MAX];
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
} MinArgs_t;

typedef struct __attribute__ ((__packed__)) PartitionTableEntry_t {
    uint8_t bootind;        //  Boot magic number (0x80 if bootable)
    uint8_t start_head;     //   Start of partition in CHS
    uint8_t start_sec;     
    uint8_t start_cyl;     
    uint8_t type;           //  Type of partition (0x81 is minix)
    uint8_t end_head;       //  head End of partition in CHS
    uint8_t end_sec;     
    uint8_t end_cyl;     
    uint32_t lFirst;        //  First sector (LBA addressing)
    uint32_t size;          //  size of partition (in sectors)
} PartitionTableEntry_t;

#define MAX_NAME_SIZE   (60)
typedef struct __attribute__ ((__packed__)) DirEntry_t {
    uint32_t inode;
    char name[MAX_NAME_SIZE];
} DirEntry_t;

typedef struct __attribute__ ((__packed__)) SuperBlock_t { 
    /* Minix Version 3 Superblock
    * this structure found in fs/super.h
    * in minix 3.1.1
    */
    /* on disk. These fields and orientation are non–negotiable */
    uint32_t ninodes;       /* number of inodes in this filesystem */
    uint16_t pad1;          /* make things line up properly */
    int16_t i_blocks;       /* # of blocks used by inode bit map */
    int16_t z_blocks;       /* # of blocks used by zone bit map */
    uint16_t firstdata;     /* number of first data zone */
    int16_t log_zone_size;  /* log2 of blocks per zone */
    int16_t pad2;           /* make things line up again */
    uint32_t max_file;      /* maximum file size */
    uint32_t zones;         /* number of zones on disk */
    int16_t magic;          /* magic number */
    int16_t pad3;           /* make things line up again */
    uint16_t blocksize;     /* block size in bytes */
    uint8_t subversion;     /* filesystem sub–version */
} SuperBlock_t;

#define DIRECT_ZONES    (7)
typedef struct __attribute__ ((__packed__)) Inode_t {
    uint16_t mode; /* mode */
    uint16_t links; /* number or links */
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
} Inode_t;

void parse_args(int argc, char** argv, bool minls, MinArgs_t* args);

void get_partition_entry(MinArgs_t* args, PartitionTableEntry_t* entry);
bool isvalid_partition_table(uint8_t* boot_block);

void get_superblock(MinArgs_t* args, PartitionTableEntry_t* entry, SuperBlock_t* sup_block);
bool isvalid_minix_fs(SuperBlock_t* sup_block);

void get_root_inode(); // TODO


// PRINTING
void print_superblock(SuperBlock_t* block);
void print_partition_entry(PartitionTableEntry_t* block);

void print_perms(uint16_t mode);
void print_dir(Inode_t inode, DirEntry_t* dir_entry);
void print_file(Inode_t inode);

extern Inode_t* inode_list;
extern int cur_inode_ind;

#endif /* min_common.h */