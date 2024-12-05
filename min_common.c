#include "min_common.h"

Inode_t* inode_list;

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

uint32_t traverse(MinArgs_t* args, uint32_t starting_inode, 
                intptr_t partition_addr, size_t zone_size) {
    char* path_copy = strdup(args->path);
    char* token = strtok(path_copy, "/");

    uint8_t* zone_buffer = (uint8_t*)malloc(zone_size);

    uint32_t cur_inode_ind = starting_inode - 1;
    DirEntry_t* cur_dir_entry = NULL;

    bool token_found = true;
    
    while (token) {
        token_found = false;
        uint32_t num_bytes_left = inode_list[cur_inode_ind].size;

        // direct zones
        for (int i = 0; i < DIRECT_ZONES && num_bytes_left > 0; i++) {
            uint32_t bytes_to_read = (num_bytes_left < zone_size) ? 
                                    num_bytes_left : zone_size;
            
            if (inode_list[cur_inode_ind].zone[i] == 0) {
                num_bytes_left -= bytes_to_read;
                continue;
            }

            intptr_t seek_addr = partition_addr + 
                (inode_list[cur_inode_ind].zone[i] * zone_size);
            fseek(args->image_file, seek_addr, SEEK_SET);
            fread(zone_buffer, sizeof(uint8_t), 
                    bytes_to_read, args->image_file);
            num_bytes_left -= bytes_to_read;

            uint32_t dirs_per_zone = bytes_to_read / sizeof(DirEntry_t);
            for (int j = 0; j < dirs_per_zone; j++) {
                cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                if (cur_dir_entry->inode == 0) continue;
                if (SAME_STR(cur_dir_entry->name,token)) {
                    token_found = true;
                    cur_inode_ind = cur_dir_entry->inode - 1;
                    break;
                }
            }

            if (token_found) break;
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
    free(zone_buffer);

    return token_found ? (cur_inode_ind + 1) : INVALID_INODE;
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

void decode_permissions(uint16_t mode, char* buf) {
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
}

// Function to print inode details
void print_inode(Inode_t* inode) {
    char perms[11];
    decode_permissions(inode->mode, perms);

    fprintf(stderr, "File inode:\n");
    fprintf(stderr, "  uint16_t mode            0x%04x (%-10s)\n", 
                    inode->mode, perms);
    fprintf(stderr, "  uint16_t links       %10u\n", inode->links);
    fprintf(stderr, "  uint16_t uid         %10u\n", inode->uid);
    fprintf(stderr, "  uint16_t gid         %10u\n", inode->gid);
    fprintf(stderr, "  uint32_t size        %10u\n", inode->size);

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


void print_dir(MinArgs_t* args, Inode_t* dir_inode, 
                intptr_t partition_addr, size_t zone_size) {
    char perms[11];
    uint8_t* zone_buffer = (uint8_t*)malloc(zone_size);
    DirEntry_t* cur_dir_entry = NULL;

    uint32_t num_bytes_left = dir_inode->size;

    // direct zones
    for (int i = 0; i < DIRECT_ZONES && num_bytes_left > 0; i++) {
        uint32_t bytes_to_read = (num_bytes_left < zone_size) ? 
                                num_bytes_left : zone_size;
        
        if (dir_inode->zone[i] == 0) {
            num_bytes_left -= bytes_to_read;
            continue;
        }

        intptr_t seek_addr = partition_addr + 
            (dir_inode->zone[i] * zone_size);
        fseek(args->image_file, seek_addr, SEEK_SET);
        fread(zone_buffer, sizeof(uint8_t), bytes_to_read, args->image_file);
        num_bytes_left -= bytes_to_read;

        uint32_t dirs_per_zone = bytes_to_read / sizeof(DirEntry_t);
        for (int j = 0; j < dirs_per_zone; j++) {
            cur_dir_entry = (DirEntry_t*)zone_buffer + j;

            if (cur_dir_entry->inode == 0) continue;

            Inode_t* inode = inode_list + (cur_dir_entry->inode - 1);
            decode_permissions(inode->mode, perms);
            
            printf("%s %9d %s\n", perms, inode->size, cur_dir_entry->name);
        }
    }

    free(zone_buffer);
}


void print_file(Inode_t* inode, const char* path) {
    char perms[11];
    decode_permissions(inode->mode, perms);
    printf("%s %9d %s\n", perms, inode->size, path);
    return;
}




