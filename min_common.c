#include "min_common.h"

Inode_t* inode_list;
size_t cur_inode_ind;

void parse_args(int argc, char** argv, bool minls, MinArgs_t* args) {
    args->partnum = -1;
    args->subnum = -1;
    
    // min args len
    if (argc < MIN_MINLS_ARGS && minls) {
        print_usage(minls);
        exit(EXIT_FAILURE);
    } 
    else if (argc < MIN_MINGET_ARGS && !minls) {
        print_usage(minls);
        exit(EXIT_FAILURE);
    }
 
    // minls [ -v ] [ -p part [ -s subpart ] ] imagefile [ path ]
    // minget [ -v ] [ -p part [ -s subpart ] ] imagefile srcpath [ dstpath ]

    char opt;
    while ((opt = getopt(argc, argv, "vp:s:")) != -1) {
        switch (opt) {
        case 'v':
            args->verbose = true;
            break;
        case 'p':
            if (strlen(optarg) == 0) {
                perror("invalid part num");
                exit(EXIT_FAILURE);
            }
            args->partnum = atoi(optarg);
            if (args->partnum < 0 || args->partnum > 3) {
                // error
            }
            break;
        case 's':
            if (strlen(optarg) == 0) {
                perror("invalid part num");
                exit(EXIT_FAILURE);
            }
            args->subnum = atoi(optarg);
            if (args->subnum < 0 || args->subnum > 3) {
                // error
            }
            break;
        default:
            print_usage(minls);
            break;
        }
    }

    if (args->partnum < 0 && args->subnum >= 0) {
        perror("cannot have subpartition without partition");
        exit(EXIT_FAILURE);
    }

    // image_file
    if (optind < argc) {
        args->image_file = fopen(argv[optind], "r");
        if (!args->image_file) {
            perror("file doesnt exist");
            exit(EXIT_FAILURE);
        }
    }

    if (minls) {
        // pathname
        if (++optind < argc) {
            strncpy(args->path, argv[optind], PATH_MAX);
        } else {
            strncpy(args->path, DEFAULT_PATH, PATH_MAX);
        }
    }
    else {
        // src_path
        strncpy(args->src_path, argv[++optind], PATH_MAX);
        if (++optind < argc) {
            // dest_path
            strncpy(args->dst_path, argv[optind], PATH_MAX);
        }
    }

    return;
}

inline bool isvalid_minix_fs(SuperBlock_t* sup_block) {
    return sup_block->magic == MINIX_MAGIC_NUM;
}

inline bool isvalid_partition_table(uint8_t* boot_block) {
    return boot_block[510] == VALID_BYTE510 && boot_block[511] == VALID_BYTE511;
}


void get_partition_entry(MinArgs_t* args, PartitionTableEntry_t* entry) {
    uint8_t block[BOOT_BLOCK_SIZE];
    size_t bytes;

    PartitionTableEntry_t* temp_pt_entry;

    if (args->partnum < 0) {
        return;
    }

    fseek(args->image_file, 0, SEEK_SET);
    bytes = fread(block, sizeof(uint8_t), BOOT_BLOCK_SIZE, args->image_file);
    if (!isvalid_partition_table(block)) {
        perror("invalid partition table");
        exit(EXIT_FAILURE);
    }

    temp_pt_entry = ((PartitionTableEntry_t*) (block + PART_TBL_OFFSET)) 
                    + args->partnum;
    if (args->subnum < 0) {
        if (temp_pt_entry->type != MINIX_PART_TYPE) {
            perror("not a minix partition type");
            exit(EXIT_FAILURE);
        }

        memcpy(entry, temp_pt_entry, sizeof(PartitionTableEntry_t));
        return;
    }

    fseek(args->image_file, temp_pt_entry->lFirst * SECTOR_SIZE, SEEK_SET);
    bytes = fread(block, sizeof(uint8_t), BOOT_BLOCK_SIZE, args->image_file);
    if (!isvalid_partition_table(block)) {
        perror("invalid partition table");
        exit(EXIT_FAILURE);
    }

    temp_pt_entry = ((PartitionTableEntry_t*)(block + PART_TBL_OFFSET)) 
                    + args->subnum;
    if (temp_pt_entry->type != MINIX_PART_TYPE) {
        perror("not a minix partition type");
        exit(EXIT_FAILURE);
    }

    memcpy(entry, temp_pt_entry, sizeof(PartitionTableEntry_t));

    return;
}

void get_superblock(MinArgs_t* args, PartitionTableEntry_t* entry, 
                    SuperBlock_t* sup_block) {
    size_t bytes;

    size_t sup_block_addr = (args->partnum >= 0) * 
        entry->lFirst * SECTOR_SIZE + SB_OFFSET;
    fseek(args->image_file, sup_block_addr, SEEK_SET);
    bytes = fread(sup_block, sizeof(SuperBlock_t), 1, args->image_file);
    
    if(!isvalid_minix_fs(sup_block)) {
        perror("MINIX magic number not found");
        exit(EXIT_FAILURE);
    }

    return;
}


uint32_t get_inode(char* target, DirEntry_t* zone, uint32_t zone_size) {
    for (int i = 0; i < zone_size; i++) {
        DirEntry_t dir_entry = zone[i];
        // printf("%u %s\n", dir_entry.inode, dir_entry.name);
        if (dir_entry.inode == 0) {
            continue;
        }

        if (SAME_STR(target, dir_entry.name)) {
            return dir_entry.inode;
        }
    }
    return 0;
}

uint32_t traverse(char* path, uint32_t starting_inode, bool is_searching) {
    
}



void print_usage(bool minls) {
    if (minls) {
        fprintf(stderr,
            "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n"
            "Options:\n"
            "-p part     --- select partition for filesystem \
            (default: none)\n"
            "-s sub      --- select subpartition for filesystem \
            (default: none)\n"
            "-h help     --- print usage information and exit\n"
            "-v verbose  --- increase verbosity level\n");
    }
    else {
        fprintf(stderr,
            "usage: minget [ -v ] [ -p part [ -s subpart ] ] imagefile \
            srcpath [ dstpath ]\n"
            "Options:\n"
            "-p part     --- select partition for filesystem \
            (default: none)\n"
            "-s sub      --- select subpartition for filesystem \
            (default: none)\n"
            "-h help     --- print usage information and exit\n"
            "-v verbose  --- increase verbosity level\n");
    }

    return;
}

void print_partition_entry(PartitionTableEntry_t* block) {
    const char* print_string = "Partition Entry:\nStored Fields: \
    \n\tbootind %d \
    \n\tstart_head %d \
    \n\tstart_sec %d \
    \n\tstart_cyl %d \
    \n\ttype %d \
    \n\tend_head %d \
    \n\tend_sec %d \
    \n\tend_cyl %d \
    \n\tlFirst %d \
    \n\tsize %d\n";

    fprintf(stderr, print_string,
        block->bootind,
        block->start_head,
        block->start_sec,
        block->start_cyl,
        block->type,
        block->end_head,
        block->end_sec,
        block->end_cyl,
        block->lFirst,
        block->size
    );
}

void print_superblock(SuperBlock_t* block) {
    const char* print_string = "Superblock Contents:\nStored Fields: \
    \n\tninodes %d \
    \n\ti_blocks %d \
    \n\tz_blocks %d \
    \n\tfirstdata %d \
    \n\tlog_zone_size %d \
    \n\tmax_file %u \
    \n\tmagic 0x%04X \
    \n\tzones %d \
    \n\tblocksize %d \
    \n\tsubversion %d\n";

    fprintf(stderr, print_string,
        block->ninodes,
        block->i_blocks,
        block->z_blocks,
        block->firstdata,
        block->log_zone_size,
        block->max_file,
        block->magic,
        block->zones,
        block->blocksize,
        block->subversion
    );
}

char* decode_permissions(uint16_t mode) {
    char* buf = (char*) malloc(sizeof(char) * 11);
    buf[0] = (mode & DIRECTORY) ? 'd' : '-'; 
    buf[1] = (mode & OWNER_READ) ? 'r' : '-'; 
    buf[2] = (mode & OWNER_WRITE) ? 'w' : '-'; 
    buf[3] = (mode & OWNER_EXEC) ? 'x' : '-'; 
    buf[4] = (mode & GROUP_READ) ? 'r' : '-'; 
    buf[5] = (mode & GROUP_WRITE) ? 'w' : '-'; 
    buf[6] = (mode & GROUP_EXEC) ? 'x' : '-'; 
    buf[7] = (mode & OTHER_READ) ? 'r' : '-'; 
    buf[8] = (mode & OTHER_WRITE) ? 'w' : '-'; 
    buf[9] = (mode & OTHER_EXEC) ? 'x' : '-'; 
    buf[10] = '\0'; 
    return buf;
}

// Function to print inode details
void print_inode(Inode_t* inode) {
    char* permissions = decode_permissions(inode->mode);

    fprintf(stderr, "File inode:\n");
    fprintf(stderr, "  uint16_t mode            0x%04x (%-10s)\n", 
                    inode->mode, permissions);
    fprintf(stderr, "  uint16_t links       %10u\n", inode->links);
    fprintf(stderr, "  uint16_t uid         %10u\n", inode->uid);
    fprintf(stderr, "  uint16_t gid         %10u\n", inode->gid);
    fprintf(stderr, "  uint32_t size        %10u\n", inode->size);

    free(permissions);

    struct tm* timeinfo;

    // Print access time
    timeinfo = localtime((const time_t*)&inode->atime);
    fprintf(stderr, "  uint32_t atime       %10d --- %s\n", 
                    inode->atime, asctime(timeinfo));

    // Print modification time
    timeinfo = localtime((const time_t*)&inode->mtime);
    fprintf(stderr, "  uint32_t mtime       %10d --- %s\n", 
                    inode->mtime, asctime(timeinfo));

    // Print creation time
    timeinfo = localtime((const time_t*)&inode->ctime);
    fprintf(stderr, "  uint32_t ctime       %10d --- %s\n", 
                    inode->ctime, asctime(timeinfo));

    // Print direct zones
    fprintf(stderr, "  Direct zones:\n");
    for (int i = 0; i < DIRECT_ZONES; i++) {
        fprintf(stderr, "\t\tzone[%d]       %10u\n", i, inode->zone[i]);
    }

    // Print indirect zones
    fprintf(stderr, "  uint32_t indirect %10u\n", inode->indirect);
    fprintf(stderr, "  uint32_t double   %10u\n", inode->two_indirect);
}


void print_dir(Inode_t dir_inode, DirEntry_t* dir_entry) {
    for(int i = 0; i < dir_inode.size / sizeof(DirEntry_t); i++) {
        if (dir_entry->inode != 0) {
            Inode_t inode = inode_list[dir_entry->inode]; 
            char* perms = decode_permissions(inode.mode);
            
            printf("%s %9d %s\n", perms, inode.size, dir_entry->name);
            free(perms);
        }

        dir_entry++;
    }

    return;
}


void print_file(Inode_t inode, const char* path) {
    char* perms = decode_permissions(inode.mode);
    printf("%s %9d %s\n", perms, inode.size, path);
    free(perms);
    return;
}




