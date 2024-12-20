#include "min_common.h"

Inode_t* inode_list;


/**
 * desc - parse the arguments for the minls and minget programs
 * argc: number of arguments
 * argv: array of arguments
 * minls: true if minls, false if minget
 * args: struct to store the parsed arguments
 */
void parse_args(int argc, char** argv, bool minls, MinArgs_t* args) {
    args->partnum = -1;
    args->subnum = -1;
    
    // min args len for programs
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
                perror("Partition Number can limited 0-3");
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            if (strlen(optarg) == 0) {
                perror("invalid part num");
                exit(EXIT_FAILURE);
            }
            args->subnum = atoi(optarg);
            if (args->subnum < 0 || args->subnum > 3) {
                perror("SubPartition Number can limited 0-3");
                exit(EXIT_FAILURE);

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

    // open the file* for image_file
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
        if (++optind < argc) {
            strncpy(args->src_path, argv[optind], PATH_MAX);
        }
        else {
            perror("src not defined");
            exit(EXIT_FAILURE);
        }
        
        // dest_path
        if (++optind < argc) {
            args->dst_file = fopen(argv[optind], "w");
        }
        else {
            args->dst_file = stdout;
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

inline bool is_dir(Inode_t* inode) {
    return inode->mode & DIRECTORY;
}

/**
 * desc - get the partition entry from the image file
 * args: struct containing the parsed arguments
 * entry: struct to store the partition entry
 */
void get_partition_entry(MinArgs_t* args, PartitionTableEntry_t* entry) {
    uint8_t block[BOOT_BLOCK_SIZE];

    PartitionTableEntry_t* temp_pt_entry;

    // unpartitioned
    if (args->partnum < 0) {
        return;
    }

    // load in the partition
    fseek(args->image_file, 0, SEEK_SET);
    fread(block, sizeof(uint8_t), BOOT_BLOCK_SIZE, args->image_file);
    if (!isvalid_partition_table(block)) {
        perror("invalid partition table");
        exit(EXIT_FAILURE);
    }

    temp_pt_entry = ((PartitionTableEntry_t*) (block + PART_TBL_OFFSET)) 
                    + args->partnum;
    // no subpartitions
    if (args->subnum < 0) {
        if (temp_pt_entry->type != MINIX_PART_TYPE) {
            perror("not a minix partition type");
            exit(EXIT_FAILURE);
        }

        memcpy(entry, temp_pt_entry, sizeof(PartitionTableEntry_t));
        return;
    }

    // load in parition table from the primary partition
    fseek(args->image_file, temp_pt_entry->lFirst * SECTOR_SIZE, SEEK_SET);
    fread(block, sizeof(uint8_t), BOOT_BLOCK_SIZE, args->image_file);
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


/**
 * desc - get the superblock from the image file
 * args: struct containing the parsed arguments
 * entry: struct containing the partition entry
 * sup_block: struct to store the superblock
 */
void get_superblock(MinArgs_t* args, PartitionTableEntry_t* entry, 
                    SuperBlock_t* sup_block) {

    // load in the superblock 
    size_t sup_block_addr = (args->partnum >= 0) * 
        entry->lFirst * SECTOR_SIZE + SB_OFFSET;
    fseek(args->image_file, sup_block_addr, SEEK_SET);
    fread(sup_block, sizeof(SuperBlock_t), 1, args->image_file);
    
    if(!isvalid_minix_fs(sup_block)) {
        perror("MINIX magic number not found");
        exit(EXIT_FAILURE);
    }

    return;
}

/**
 * desc - traverse the filesystem to find the inode of the file
 * fp: file pointer to the image file
 * path: path to the file
 * starting_inode: inode to start the search from
 * partition_addr: address of the partition
 * zone_size: size of a zone
 * block_size: size of a block
 * return: inode number of the file or INVALID_INODE if not found
 */
uint32_t traverse(FILE* fp, char* path, uint32_t starting_inode, 
                intptr_t partition_addr, 
                size_t zone_size, size_t block_size) {
    
    // make a copy to work with
    char* path_copy = strdup(path);
    char* token = strtok(path_copy, "/");

    uint8_t* zone_buffer = (uint8_t*)malloc(zone_size);
    uint32_t zone_array[INDIRECT_ZONES];
    uint32_t indirect_zone_array[INDIRECT_ZONES];

    uint32_t cur_inode_ind = starting_inode - 1;
    Inode_t* cur_inode = (inode_list + cur_inode_ind);
    DirEntry_t* cur_dir_entry = NULL;

    bool token_found = true;
    bool is_dir = true;

    while (token) {
        if (!is_dir) {
            perror("trying to access file as directory");
            return INVALID_INODE;
        }

        token_found = false;
        uint32_t num_bytes_left = cur_inode->size;

        // direct zones
        for (int i = 0; i < DIRECT_ZONES && num_bytes_left > 0; i++) {
            uint32_t bytes_to_read = (num_bytes_left < zone_size) ? 
                                    num_bytes_left : zone_size;
            
            if (cur_inode->zone[i] == 0) {
                num_bytes_left -= bytes_to_read;
                continue;
            }

            intptr_t seek_addr = partition_addr + 
                (cur_inode->zone[i] * zone_size);
            fseek(fp, seek_addr, SEEK_SET);
            fread(zone_buffer, sizeof(uint8_t), 
                    bytes_to_read, fp);
            num_bytes_left -= bytes_to_read;

            uint32_t dirs_per_zone = bytes_to_read / sizeof(DirEntry_t);
            for (int j = 0; j < dirs_per_zone; j++) {
                cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                if (cur_dir_entry->inode == 0) continue;
                if (SAME_STR(cur_dir_entry->name,token)) {
                    token_found = true;

                    cur_inode_ind = cur_dir_entry->inode - 1;
                    cur_inode = inode_list + cur_inode_ind;

                    is_dir = cur_inode->mode & DIRECTORY;
                    break;
                }
            }

            if (token_found) break;
        }

        // indirect zones
        if (num_bytes_left > 0 && !token_found) {
            if (cur_inode->indirect == 0) {
                uint32_t num_holes = (INDIRECT_ZONES * block_size);
                uint32_t bytes_to_read = (num_bytes_left < num_holes) ? 
                    num_bytes_left : num_holes;
                num_bytes_left -= bytes_to_read;
            }
            else {
                intptr_t indirect_zone_array_addr = partition_addr + 
                    (cur_inode->indirect * zone_size);
                fseek(fp, indirect_zone_array_addr, SEEK_SET);
                fread(
                    zone_array, 
                    sizeof(uint32_t), 
                    INDIRECT_ZONES, 
                    fp
                );

                for(int i = 0; i < INDIRECT_ZONES && num_bytes_left > 0; i++) {
                    uint32_t bytes_to_read = (num_bytes_left < block_size) ? 
                        num_bytes_left : block_size;
                    
                    if (zone_array[i] == 0) {
                        num_bytes_left -= bytes_to_read;
                        continue;
                    }

                    intptr_t seek_addr = partition_addr + 
                        (zone_array[i] * zone_size);
                    fseek(fp, seek_addr, SEEK_SET);
                    fread(
                        zone_buffer, 
                        sizeof(uint8_t), 
                        bytes_to_read, 
                        fp
                    );
                    num_bytes_left -= bytes_to_read;

                    uint32_t dirs_per_zone = 
                        bytes_to_read / sizeof(DirEntry_t);
                    for (int j = 0; j < dirs_per_zone; j++) {
                        cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                        if (cur_dir_entry->inode == 0) continue;
                        if (SAME_STR(cur_dir_entry->name,token)) {
                            token_found = true;

                            cur_inode_ind = cur_dir_entry->inode - 1;
                            cur_inode = inode_list + cur_inode_ind;

                            is_dir = 
                                cur_inode->mode & DIRECTORY;
                            break;
                        }
                    }

                    if (token_found) break;
                }
            }
        }
        
        // double indirect zones
        if (num_bytes_left > 0 && !token_found) {
            if (cur_inode->two_indirect == 0) {
                uint32_t num_holes = 
                    (INDIRECT_ZONES * INDIRECT_ZONES * block_size);
                uint32_t bytes_to_read = (num_bytes_left < num_holes) ? 
                    num_bytes_left : num_holes;
                num_bytes_left -= bytes_to_read;
            }
            else {
                intptr_t double_indirect_zone_array_addr = partition_addr + 
                    (cur_inode->two_indirect * zone_size);
                fseek(fp, double_indirect_zone_array_addr, SEEK_SET);
                fread(indirect_zone_array, 
                    sizeof(uint32_t), INDIRECT_ZONES, fp);

                for (int k = 0; k < INDIRECT_ZONES; k++) {
                    if (indirect_zone_array[k] == 0) {
                        uint32_t num_holes = (INDIRECT_ZONES * block_size);
                        uint32_t bytes_to_read = (num_bytes_left < num_holes) ?
                            num_bytes_left : num_holes;
                        num_bytes_left -= bytes_to_read;
                        continue;
                    }
                    else {
                        intptr_t indirect_zone_array_addr = partition_addr + 
                            (indirect_zone_array[k] * zone_size);
                        fseek(fp, indirect_zone_array_addr, SEEK_SET);
                        fread(zone_array, 
                            sizeof(uint32_t), INDIRECT_ZONES, fp);

                        for (int i = 0; i < INDIRECT_ZONES; i++) {
                            uint32_t bytes_to_read = 
                                (num_bytes_left < block_size) ? 
                                    num_bytes_left : block_size;
                            
                            if (zone_array[i] == 0) {
                                num_bytes_left -= bytes_to_read;
                                continue;
                            }

                            intptr_t seek_addr = partition_addr + 
                                (zone_array[i] * zone_size);
                            fseek(fp, seek_addr, SEEK_SET);
                            fread(zone_buffer, 
                                sizeof(uint8_t), bytes_to_read, fp);
                            num_bytes_left -= bytes_to_read;

                            uint32_t dirs_per_zone = 
                                bytes_to_read / sizeof(DirEntry_t);
                            for (int j = 0; j < dirs_per_zone; j++) {
                                cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                                if (cur_dir_entry->inode == 0) continue;
                                if (SAME_STR(cur_dir_entry->name,token)) {
                                    token_found = true;
                                    
                                    cur_inode_ind = cur_dir_entry->inode - 1;
                                    cur_inode = inode_list + cur_inode_ind;

                                    is_dir = 
                                        cur_inode->mode & DIRECTORY;

                                    break;
                                }
                            }

                            if (token_found) break;
                        }

                        if (token_found) break;
                    }
                }
            }
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

    // access time
    timeinfo = localtime((const time_t*)&inode->atime);
    fprintf(stderr, "  uint32_t atime       %10d --- %s\n", 
                    inode->atime, asctime(timeinfo));

    // modification time
    timeinfo = localtime((const time_t*)&inode->mtime);
    fprintf(stderr, "  uint32_t mtime       %10d --- %s\n", 
                    inode->mtime, asctime(timeinfo));

    // creation time
    timeinfo = localtime((const time_t*)&inode->ctime);
    fprintf(stderr, "  uint32_t ctime       %10d --- %s\n", 
                    inode->ctime, asctime(timeinfo));

    // direct zones
    fprintf(stderr, "  Direct zones:\n");
    for (int i = 0; i < DIRECT_ZONES; i++) {
        fprintf(stderr, "\t\tzone[%d]       %10u\n", i, inode->zone[i]);
    }

    // indirect zones
    fprintf(stderr, "  uint32_t indirect %10u\n", inode->indirect);
    fprintf(stderr, "  uint32_t double   %10u\n", inode->two_indirect);
}


