#include "min_common.h"

int main(int argc, char** argv) {
    MinArgs_t args;
    SuperBlock_t super_block;
    PartitionTableEntry_t pt_entry;
    Inode_t root_inode;
    size_t bytes;
    
    // parse args
    parse_args(argc, argv, true, &args);

    get_superblock(&args, &super_block, &pt_entry);
    if(!isvalid_minix_fs(&super_block)) {
        perror("MINIX magic number not found");
        exit(EXIT_FAILURE);
    }

    size_t root_inode_addr = (pt_entry.lFirst * SECTOR_SIZE) +
        (2 + super_block.i_blocks + super_block.z_blocks) * super_block.blocksize;
    
    fseek(args.image_file, root_inode_addr, SEEK_SET);
    bytes = fread(&root_inode, sizeof(Inode_t), 1, args.image_file);

    return 0;
}