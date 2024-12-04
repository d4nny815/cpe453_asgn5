#include "min_common.h"

int main(int argc, char** argv) {
    size_t bytes;

    MinArgs_t args;
    PartitionTableEntry_t pt_entry;
    SuperBlock_t super_block;
    Inode_t root_inode;
    
    parse_args(argc, argv, true, &args);

    get_partition_entry(&args, &pt_entry);
    get_superblock(&args, &pt_entry, &super_block);

    print_partition_entry(&pt_entry);
    print_superblock(&super_block);

    size_t partition_addr = pt_entry.lFirst * SECTOR_SIZE;
    size_t root_inode_addr = (partition_addr) +
        (2 + super_block.i_blocks + super_block.z_blocks) * super_block.blocksize;

    inode_list = (Inode_t*) malloc(sizeof(Inode_t) * super_block.ninodes);
    cur_inode_ind = 1;
    fseek(args.image_file, root_inode_addr, SEEK_SET);
    bytes = fread(&root_inode, sizeof(Inode_t), 1, args.image_file);

    size_t zone_size = super_block.blocksize << super_block.log_zone_size;
    DirEntry_t* dir_entry = (DirEntry_t*) malloc(zone_size);
    fseek(args.image_file, partition_addr + (zone_size * inode_list[cur_inode_ind - 1].zone[0]), SEEK_SET);
    fread(dir_entry, zone_size, 1, args.image_file);

    print_dir(inode_list[cur_inode_ind - 1], dir_entry);

    
    free(inode_list);
    free(dir_entry);
    fclose(args.image_file);

    return 0;
}