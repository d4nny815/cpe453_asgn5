#include "min_common.h"

int main(int argc, char** argv) {
    MinArgs_t args;
    SuperBlock_t super_block;
    PartitionTableEntry_t pt_entry;
    Inode_t root_inode;
    size_t bytes;
    
    // parse args
    parse_args(argc, argv, true, &args);

    get_partition_entry(&args, &pt_entry);
    get_superblock(&args, &pt_entry, &super_block);

    print_partition_entry(&pt_entry);
    print_superblock(&super_block);

    size_t root_inode_addr = (pt_entry.lFirst * SECTOR_SIZE) +
        (2 + super_block.i_blocks + super_block.z_blocks) * super_block.blocksize;
    
    fseek(args.image_file, root_inode_addr, SEEK_SET);
    bytes = fread(&root_inode, sizeof(Inode_t), 1, args.image_file);

    print_perms(root_inode.mode);

    return 0;
}

void print_perms(uint16_t mode) {
    // Print permissions
    printf((mode & DIRECTORY) ? "d" : "-");
    printf((mode & OWNER_READ) ? "r" : "-");
    printf((mode & OWNER_WRITE) ? "w" : "-");
    printf((mode & OWNER_EXEC) ? "x" : "-");
    printf((mode & GROUP_READ) ? "r" : "-");
    printf((mode & GROUP_WRITE) ? "w" : "-");
    printf((mode & GROUP_EXEC) ? "x" : "-");
    printf((mode & OTHER_READ) ? "r" : "-");
    printf((mode & OTHER_WRITE) ? "w" : "-");
    printf((mode & OTHER_EXEC) ? "x" : "-");
    printf("\n");
}

void print_dir() {

}