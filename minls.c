#include "min_common.h"

void print_file();
void print_dir();

Inode_t* inode_list;

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

    size_t partition_addr = pt_entry.lFirst * SECTOR_SIZE;

    size_t root_inode_addr = (partition_addr) +
        (2 + super_block.i_blocks + super_block.z_blocks) * super_block.blocksize;

    inode_list = (Inode_t*) root_inode_addr;
    
    fseek(args.image_file, root_inode_addr, SEEK_SET);
    bytes = fread(&root_inode, sizeof(Inode_t), 1, args.image_file);

    size_t zone_size = super_block.blocksize << super_block.log_zone_size;

    DirEntry_t* dir_entry = (DirEntry_t*) malloc(zone_size);
    fseek(args.image_file, partition_addr + (zone_size * root_inode.zone[0]), SEEK_SET);
    fread(dir_entry, zone_size, 1, args.image_file);

    print_dir(root_inode, dir_entry);

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
}

void print_dir(Inode_t inode, DirEntry_t* dir_entry) {
    for(int i = 0; i < inode.links + 2; i++) {
        print_perms(inode.mode);
        printf("%10d %s\n", inode.size, dir_entry->name);
        dir_entry++;
    }

}

void print_file(Inode_t inode) {
    printf("%o\n", inode.mode);
    print_perms(inode.mode);

}