#include "min_common.h"

void print_file();
void print_dir();
char** split_path(const char* path);
void free_split_path(char** path_components);



Inode_t* inode_list;
size_t cur_inode_ind;

int main(int argc, char** argv) {
    size_t bytes;

    MinArgs_t args;
    PartitionTableEntry_t pt_entry;
    SuperBlock_t super_block;
    Inode_t root_inode;
    
    // parse args and put them into a struct
    parse_args(argc, argv, true, &args);

    get_partition_entry(&args, &pt_entry);
    get_superblock(&args, &pt_entry, &super_block);

    print_partition_entry(&pt_entry);
    print_superblock(&super_block);
    // get the super block asked for
    if(!isvalid_minix_fs(&super_block)) {
        perror("MINIX magic number not found");
        exit(EXIT_FAILURE);
    }

    // start of the "partition"
    size_t partition_addr = pt_entry.lFirst * SECTOR_SIZE;
    size_t root_inode_addr = (partition_addr) +
        (2 + super_block.i_blocks + super_block.z_blocks) * super_block.blocksize;

    // load in inode list
    inode_list = (Inode_t*) malloc(sizeof(Inode_t) * super_block.ninodes);
    cur_inode_ind = ROOT_INODE;
    fseek(args.image_file, root_inode_addr, SEEK_SET);
    bytes = fread(&root_inode, sizeof(Inode_t), 1, args.image_file);

    // split the path
    size_t zone_size = super_block.blocksize << super_block.log_zone_size;

    char* path_copy = strdup(args.path);
    char* token = strtok(path_copy, "/");
    DirEntry_t* dir_entries = (DirEntry_t*) malloc(zone_size);
    while (token) {
        size_t zone_addr = partition_addr + (zone_size * inode_list[cur_inode_ind - 1].zone[0]);
        fseek(args.image_file, zone_addr, SEEK_SET);
        fread(dir_entries, zone_size, 1, args.image_file);

        // search for inode
        int inodes_size = inode_list[cur_inode_ind - 1].size / sizeof(DirEntry_t);
        cur_inode_ind = get_inode(token, dir_entries, inodes_size);

        if (cur_inode_ind == 0) {
            fprintf(stderr, "File %s not found!\n", args.path);
            exit(1);
        }


        token = strtok(NULL, "/");
    }

    Inode_t file_inode = inode_list[cur_inode_ind - 1];
    if (file_inode.mode & DIRECTORY) {
        print_dir(file_inode, dir_entries);
    } 
    else {
        print_file(file_inode);
    }

    // print_file(cur_inode_ind);
    // print_dir(cur_inode_ind, dir)

    // walk the path
    // if file
        // print file
    // else if dir
        // print dir
    // ?something for special file

    // clean up and bring er home 


    // walk the path
    free(inode_list);
    fclose(args.image_file);

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
    for(int i = 0; i < inode.size / sizeof(DirEntry_t); i++) {
        if (dir_entry->inode != 0) { // TODO: invalid inode
            print_perms(inode_list[dir_entry->inode - 1].mode);
            printf("%10d %s\n", inode_list[dir_entry->inode - 1].size, dir_entry->name);
        }

        dir_entry++;
    }

    return;
}


void print_file(Inode_t inode) {
    // TODO
    
    return;
}
