#include "min_common.h"

Inode_t* inode_list;
size_t cur_inode_ind;

void print_file(Inode_t* inode, const char* path);
void print_dir(MinArgs_t* args, Inode_t* dir_inode, 
                intptr_t partition_addr, 
                size_t zone_size, size_t block_size);


int main(int argc, char** argv) {
    MinArgs_t args;
    PartitionTableEntry_t pt_entry;
    SuperBlock_t super_block;
    
    int err;

    // parse args and put them into a struct
    parse_args(argc, argv, true, &args);

    get_partition_entry(&args, &pt_entry);
    get_superblock(&args, &pt_entry, &super_block);

    // start of the "partition"
    size_t partition_addr = pt_entry.lFirst * SECTOR_SIZE;
    size_t root_inode_addr = (partition_addr) +
        (2 + super_block.i_blocks + super_block.z_blocks) * 
        super_block.blocksize;

    size_t zone_size = super_block.blocksize << super_block.log_zone_size;

    // load in inode list
    inode_list = (Inode_t*) malloc(sizeof(Inode_t) * super_block.ninodes);
    
    err = fseek(args.image_file, root_inode_addr, SEEK_SET);
    if (err == -1) {
        exit(EXIT_FAILURE);
    }
    fread(inode_list, sizeof(Inode_t), 
        super_block.ninodes, 
        args.image_file
    );

    uint32_t found_inode_num = traverse(
        args.image_file,
        args.path,
        ROOT_INODE, 
        partition_addr,
        zone_size, 
        super_block.blocksize
    );

    if (found_inode_num == INVALID_INODE) {
        perror("file not found");
        exit(EXIT_FAILURE);
    }

    Inode_t* found_inode = inode_list + (found_inode_num-1);

    if (args.verbose) {
        printf("\n");
        print_superblock(&super_block);
        printf("\n");
        print_inode(found_inode);
        printf("\n");
    }

    if (found_inode->mode & DIRECTORY) {
        printf("%s:\n", args.path);
        print_dir(
            &args, found_inode, partition_addr, 
            zone_size, super_block.blocksize
        );
    } 
    else {
        print_file(found_inode, args.path);
    }


    // walk the path
    free(inode_list);
    fclose(args.image_file);

    return 0;
}


void print_dir(MinArgs_t* args, Inode_t* dir_inode, 
                intptr_t partition_addr, 
                size_t zone_size, size_t block_size) {
    
    uint8_t* zone_buffer = (uint8_t*)malloc(zone_size);
    uint32_t zone_array[INDIRECT_ZONES];
    uint32_t indirect_zone_array[INDIRECT_ZONES];

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
            print_file(inode, cur_dir_entry->name);
        }
    }

    // indirect zones
    if (num_bytes_left > 0) {
        if (dir_inode->indirect == 0) {
            uint32_t num_holes = (INDIRECT_ZONES * block_size);
            uint32_t bytes_to_read = (num_bytes_left < num_holes) ? 
                num_bytes_left : num_holes;
            num_bytes_left -= bytes_to_read;
        }
        else {
            intptr_t indirect_zone_array_addr = partition_addr + 
                (dir_inode->indirect * zone_size);
            fseek(args->image_file, indirect_zone_array_addr, SEEK_SET);
            fread(
                zone_array, 
                sizeof(uint32_t), 
                INDIRECT_ZONES, 
                args->image_file
            );

            for (int i = 0; i < INDIRECT_ZONES && num_bytes_left > 0; i++) {
                uint32_t bytes_to_read = (num_bytes_left < block_size) ? 
                    num_bytes_left : block_size;
                
                if (zone_array[i] == 0) {
                    num_bytes_left -= bytes_to_read;
                    continue;
                }

                intptr_t seek_addr = partition_addr + 
                    (zone_array[i] * zone_size);
                fseek(args->image_file, seek_addr, SEEK_SET);
                fread(
                    zone_buffer, 
                    sizeof(uint8_t), 
                    bytes_to_read, 
                    args->image_file
                );
                num_bytes_left -= bytes_to_read;

                uint32_t dirs_per_zone = bytes_to_read / sizeof(DirEntry_t);
                for (int j = 0; j < dirs_per_zone; j++) {
                    cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                    if (cur_dir_entry->inode == 0) continue;

                    Inode_t* inode = inode_list + (cur_dir_entry->inode - 1);
                    print_file(inode, cur_dir_entry->name);
                }
            }
        }
    }

    // double indirect zones
    if (num_bytes_left > 0) {
        if (dir_inode->two_indirect == 0) {
            uint32_t num_holes = 
                (INDIRECT_ZONES * INDIRECT_ZONES * block_size);
            uint32_t bytes_to_read = (num_bytes_left < num_holes) ? 
                num_bytes_left : num_holes;
            num_bytes_left -= bytes_to_read;
        }
        else {
            intptr_t double_indirect_zone_array_addr = partition_addr + 
                (dir_inode->two_indirect * zone_size);
            fseek(args->image_file, double_indirect_zone_array_addr, SEEK_SET);
            fread(indirect_zone_array, 
                sizeof(uint32_t), INDIRECT_ZONES, args->image_file);

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
                    fseek(args->image_file, 
                        indirect_zone_array_addr, SEEK_SET);
                    fread(zone_array, 
                        sizeof(uint32_t), INDIRECT_ZONES, args->image_file);

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
                        fseek(args->image_file, seek_addr, SEEK_SET);
                        fread(zone_buffer, 
                            sizeof(uint8_t), bytes_to_read, args->image_file);
                        num_bytes_left -= bytes_to_read;

                        uint32_t dirs_per_zone = 
                            bytes_to_read / sizeof(DirEntry_t);
                        for (int j = 0; j < dirs_per_zone; j++) {
                            cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                            if (cur_dir_entry->inode == 0) continue;
                            
                            Inode_t* inode = inode_list + 
                                (cur_dir_entry->inode - 1);
                            
                            print_file(inode, cur_dir_entry->name);
                        }
                    }
                }
            }
        }
    }

    free(zone_buffer);

    return;
}


void print_file(Inode_t* inode, const char* path) {
    char perms[11];
    char fname[61];
    bool copied;
    if (path[59] != '\0') {
        copied = true;
        memcpy(fname, path, 60);
        fname[60] = '\0';
    }
    decode_permissions(inode->mode, perms);
    printf("%s %9d %s\n", perms, inode->size, copied ? fname : path);
    return;
}

