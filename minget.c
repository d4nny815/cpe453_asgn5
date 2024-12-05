#include "min_common.h"

Inode_t* inode_list;
size_t cur_inode_ind;

void print_file_contents(Inode_t* inode, FILE* disk_fp, 
    size_t zone_size, intptr_t partition_addr, size_t block_size,
    FILE* dest_fp);


int main(int argc, char** argv) {
    MinArgs_t args;
    PartitionTableEntry_t pt_entry;
    SuperBlock_t super_block;
    
    int err;

    // parse args and put them into a struct
    parse_args(argc, argv, false, &args);

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
        args.src_path, 
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
    
    if ((found_inode->mode & FILE_TYPE_MASK) == SYM_LINK) {
        perror("symlinks do not exist");
        exit(EXIT_FAILURE);
    } 
    if ((found_inode->mode & FILE_TYPE_MASK) != REGULAR_FILE) {
        perror("not a file");
        exit(EXIT_FAILURE);
    } 
    print_file_contents(found_inode, args.image_file, 
        zone_size, partition_addr, super_block.blocksize, args.dst_file);


    // walk the path
    free(inode_list);
    fclose(args.image_file);
    fclose(args.dst_file);

    return 0;
}


void print_file_contents(Inode_t* inode, FILE* disk_fp, 
        size_t zone_size, intptr_t partition_addr, size_t block_size, 
        FILE* dest_fp) {
    
    uint8_t* zone_buffer = (uint8_t*)malloc(zone_size);
    uint32_t zone_array[INDIRECT_ZONES];
    uint32_t indirect_zone_array[INDIRECT_ZONES];

    uint32_t num_bytes_left = inode->size;

    // direct zones
    for (int i = 0; i < DIRECT_ZONES && num_bytes_left > 0; i++) {
        uint32_t bytes_to_read = (num_bytes_left < zone_size) ? 
                                num_bytes_left : zone_size;
        
        if (inode->zone[i] == 0) {
            memset(zone_buffer, 0, bytes_to_read);
            fwrite(zone_buffer, sizeof(char), bytes_to_read, dest_fp);

            num_bytes_left -= bytes_to_read;
            continue;
        }

        intptr_t seek_addr = partition_addr + 
            (inode->zone[i] * zone_size);
        fseek(disk_fp, seek_addr, SEEK_SET);
        fread(zone_buffer, sizeof(uint8_t), bytes_to_read, disk_fp);

        fwrite(zone_buffer, sizeof(char), bytes_to_read, dest_fp);
        num_bytes_left -= bytes_to_read;
    }

    // indirect zones
    if (num_bytes_left > 0) {
        if (inode->indirect == 0) {
            uint32_t num_holes = (INDIRECT_ZONES * block_size);
            uint32_t bytes_to_read = (num_bytes_left < num_holes) ? 
                num_bytes_left : num_holes;
            
            memset(zone_buffer, 0, block_size);
            
            uint32_t num_blocks = bytes_to_read / block_size;
            for (int i = 0; i < num_blocks; i++) {
                fwrite(zone_buffer, sizeof(char), block_size, dest_fp);
            }
            uint32_t rem = bytes_to_read - num_blocks * block_size;
            if (rem != 0) {
                fwrite(zone_buffer, sizeof(char), rem, dest_fp);
            }
            
            num_bytes_left -= bytes_to_read;
        }
        else {
            intptr_t indirect_zone_array_addr = partition_addr + 
                (inode->indirect * zone_size);
            fseek(disk_fp, indirect_zone_array_addr, SEEK_SET);
            fread(
                zone_array, 
                sizeof(uint32_t), 
                INDIRECT_ZONES, 
                disk_fp
            );

            for (int i = 0; i < INDIRECT_ZONES && num_bytes_left > 0; i++) {
                uint32_t bytes_to_read = (num_bytes_left < block_size) ? 
                    num_bytes_left : block_size;
                
                if (zone_array[i] == 0) {
                    memset(zone_buffer, 0, bytes_to_read);
                    fwrite(zone_buffer, sizeof(char), bytes_to_read, dest_fp);

                    num_bytes_left -= bytes_to_read;
                    continue;
                }

                intptr_t seek_addr = partition_addr + 
                    (zone_array[i] * zone_size);
                fseek(disk_fp, seek_addr, SEEK_SET);
                fread(
                    zone_buffer, 
                    sizeof(uint8_t), 
                    bytes_to_read, 
                    disk_fp
                );
                
                fwrite(zone_buffer, sizeof(char), bytes_to_read, dest_fp);
                num_bytes_left -= bytes_to_read;
            }
        }
    }

    // double indirect zones
    if (num_bytes_left > 0) {
        if (inode->two_indirect == 0) {
            uint32_t num_holes = 
                (INDIRECT_ZONES * INDIRECT_ZONES * block_size);

            uint32_t bytes_to_read = (num_bytes_left < num_holes) ? 
                num_bytes_left : num_holes;
            
            memset(zone_buffer, 0, block_size);
            
            uint32_t num_blocks = bytes_to_read / block_size;
            for (int i = 0; i < num_blocks; i++) {
                fwrite(zone_buffer, sizeof(char), block_size, dest_fp);
            }
            uint32_t rem = bytes_to_read - num_blocks * block_size;
            if (rem != 0) {
                fwrite(zone_buffer, sizeof(char), rem, dest_fp);
            }

            num_bytes_left -= bytes_to_read;
        }
        else {
            intptr_t double_indirect_zone_array_addr = partition_addr + 
                (inode->two_indirect * zone_size);
            fseek(disk_fp, double_indirect_zone_array_addr, SEEK_SET);
            fread(indirect_zone_array, 
                sizeof(uint32_t), INDIRECT_ZONES, disk_fp);

            for (int k = 0; k < INDIRECT_ZONES; k++) {
                if (indirect_zone_array[k] == 0) {
                    uint32_t num_holes = (INDIRECT_ZONES * block_size);
                    uint32_t bytes_to_read = (num_bytes_left < num_holes) ?
                        num_bytes_left : num_holes;
                    memset(zone_buffer, 0, block_size);
            
                    uint32_t num_blocks = bytes_to_read / block_size;
                    for (int i = 0; i < num_blocks; i++) {
                        fwrite(zone_buffer, sizeof(char), block_size, dest_fp);
                    }
                    uint32_t rem = bytes_to_read - num_blocks * block_size;
                    if (rem != 0) {
                        fwrite(zone_buffer, sizeof(char), rem, dest_fp);
                    }

                    num_bytes_left -= bytes_to_read;
                    continue;
                }
                else {
                    intptr_t indirect_zone_array_addr = partition_addr + 
                        (indirect_zone_array[k] * zone_size);
                    fseek(disk_fp, 
                        indirect_zone_array_addr, SEEK_SET);
                    fread(zone_array, 
                        sizeof(uint32_t), INDIRECT_ZONES, disk_fp);

                    for (int i = 0; i < INDIRECT_ZONES; i++) {
                        uint32_t bytes_to_read = 
                            (num_bytes_left < block_size) ? 
                                num_bytes_left : block_size;
                        
                        if (zone_array[i] == 0) {
                            memset(zone_buffer, 0, bytes_to_read);
                            fwrite(zone_buffer, 
                                sizeof(char), bytes_to_read, dest_fp);
                            num_bytes_left -= bytes_to_read;
                            continue;
                        }

                        intptr_t seek_addr = partition_addr + 
                            (zone_array[i] * zone_size);
                        fseek(disk_fp, seek_addr, SEEK_SET);
                        fread(zone_buffer, 
                            sizeof(uint8_t), bytes_to_read, disk_fp);
                        
                        fwrite(zone_buffer, 
                            sizeof(char), bytes_to_read, dest_fp);
                        num_bytes_left -= bytes_to_read;
                    }
                }
            }
        }
    }

    free(zone_buffer);

    return;
}


