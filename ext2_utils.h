/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include "ext2.h"


#define DISK_SUPER_BLOCK(disk) ((struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1))

#define DISK_GROUP_DESC(disk) ((struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2))

#define DISK_BLOCK_BITMAP(disk) ((unsigned char *)(disk + EXT2_BLOCK_SIZE * DISK_GROUP_DESC(disk)->bg_block_bitmap))

#define DISK_INODE_BITMAP(disk) ((unsigned char *)(disk + EXT2_BLOCK_SIZE * DISK_GROUP_DESC(disk)->bg_inode_bitmap))

#define DISK_INODE_TABLE(disk) ((struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * DISK_GROUP_DESC(disk)->bg_inode_table))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/**
 * Memory maps the disk image and returns a pointer to the beginning of the disk.
 */
unsigned char *load_disk(char *path);


/**
 * Returns the pointer to an inode by offsetting based on the inode index.
 * 
 * Note: inode index starts at 0.
 */
struct ext2_inode *inode_from_index(unsigned char *disk, unsigned int inode);


/**
 * Returns the pointer to a directory entry by offsetting based on the block index.
 */
struct ext2_dir_entry *dir_entry_from_index(unsigned char *disk, unsigned int block);


/**
 * Returns true if the inode number is reserved.
 * 
 * Note: inode index starts at 0.
 */
bool is_inode_reserved(unsigned int inode);


/**
 * For each datablock in the inode, the callback is called with disk pointer, inode number, block
 * number, and arg.
 */
void inode_block_foreach(unsigned char *disk, unsigned int inode, void (*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg);
bool inode_block_foreach_helper(unsigned char *disk, unsigned int inode, unsigned int *i_block, unsigned int blocks, unsigned int indirection, void (*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg);

/**
 * For each datablock in the inode, the callback is called with the disk pointer, inode number, and
 * arg.
 * 
 * If callback returns true, then the find returns the datablock which made the callback return
 * true.
 * 
 * Returns -1 if the find fails.
 */
struct ext2_dir_entry *inode_dir_entry_find(unsigned char *disk, unsigned int inode, struct ext2_dir_entry *(*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg);
struct ext2_dir_entry *inode_dir_entry_find_helper(unsigned char *disk, unsigned int inode, unsigned int *iblocks_tbl, unsigned int nblocks, unsigned int indirection, struct ext2_dir_entry *(*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg);

/**
 * For each inode on the disk, the callback is called with the disk pointer, inode number and arg.
 * 
 * Use `is_inode_free(inode)` to to check whether or not the inode is in use.
 */
void inode_foreach(unsigned char *disk, void (*callback)(unsigned char *, unsigned int, void *), void *arg);


/**
 * For each block on the disk, the callback is called with the disk pointer, block number, and arg.
 * 
 * Use `is_block_free(block)` to to check whether or not the inode is in use.
 */
void block_foreach(unsigned char *disk, void (*callback)(unsigned char *, unsigned int, void *), void *arg);

/**
 * For each inode on the disk, the callback is called with the disk pointer, inode number, and arg.
 * 
 * If callback returns true, then the find returns the inode number which made the callback return
 * true.
 * 
 * Returns -1 if the find fails.
 */
unsigned int inode_find(unsigned char *disk, bool (*callback)(unsigned char *, unsigned int, void *), void *arg);

/**
 * For each block on the disk, the callback is called with the disk pointer, block number, and arg.
 * 
 * If callback returns true, then the find returns the block number which made the callback return
 * true.
 * 
 * Returns -1 if the find fails.
 */
unsigned int block_find(unsigned char *disk, bool (*callback)(unsigned char *, unsigned int, void *), void *arg);

/**
 * For each file in the directory, the callback is called with the disk pointer, directory entry,
 * and arg.
 */
void directory_entry_foreach(unsigned char *disk, unsigned int inode, void (*callback)(unsigned char *, struct ext2_dir_entry *, void *), void *arg);

void directory_entry_foreach_helper(unsigned char *disk, unsigned int inode, unsigned int block, void *arg);
struct directory_entry_foreach_helper_arg {
	void (*callback)(unsigned char *, struct ext2_dir_entry *, void *);
	void *arg;
};

/**
 * Returns whether or not the inode is used.
 */
bool is_inode_free(unsigned char *disk, unsigned int inode);

/**
 * Returns whether or not the block is used.
 */
bool is_block_free(unsigned char *disk, unsigned int block);

/**
 * Returns the index of the next free inode.
 */
unsigned int next_free_inode(unsigned char *disk);

bool next_free_inode_helper(unsigned char *disk, unsigned int inode, void *arg);

/**
 * Returns the index of the next free datablock.
 */
unsigned int next_free_block(unsigned char *disk);

bool next_free_block_helper(unsigned char *disk, unsigned int block, void *arg);

/**
 * Aligns the rec_len to the 4 byte boundary by increasing it.
 */
unsigned short rec_len_boundary(unsigned short rec_len);

/**
 * Returns whether or not the path is an absolute path
 */
bool is_abs_path(char *path);

/**
 * Removes the trailing slash
 */
void trim_trailing_slash(char *path);

/**
 * Given an abspath, returns the path leading up to the filename.
 */
char *get_filepath(char *abspath);

/**
 * Given an abspath, returns the filename.
 */
char *get_filename(char *abspath);

/**
 * Returns the inode number given an absolute path.
 * 
 * Follows the directory entries from the root directory.
 * 
 * If a directory entry does not exist, then -1 is returned.
 */
unsigned int inode_by_filepath(unsigned char *disk, char *abspath);
struct ext2_dir_entry *inode_by_filepath_helper(unsigned char *disk, unsigned int inode, unsigned int block, void *arg);

/**
 * Modifies the absolute path pointer to point to the next / and returns the filename of the
 * file that was just shifted.
 */
char *shift_filepath(char **abspath);

/**
 * Looks for a directory entry with a given name and returns it.
 * 
 * Returns NULL if no such directory is found.
 */
struct ext2_dir_entry *dir_entry_by_name(struct ext2_dir_entry *dir_entry, char *filename);

/**
 * Looks for a directory entry with a given name and returns the previous dir_entry.
 * 
 * If the dir_entry is the first in the block, then it is returned.
 * 
 * Returns NULL if no such directory is found.
 * 
 * IMPORTANT:
 * Check if the result is not NULL and if the dir_entry->name matches the filename.
 */
struct ext2_dir_entry *dir_entry_before_name(struct ext2_dir_entry *dir_entry, char *filename);

/**
 * Sets the nth bit in the bitmap to 1.
 */
void set_bit_by_index(unsigned char *bitmap, unsigned int n);

/**
 * Sets the nth bit in the bitmap to 1.
 */
void unset_bit_by_index(unsigned char *bitmap, unsigned int n);

/**
 * Returns true if the nth bit in the bitmap is set.
 */
bool is_bit_set_by_index(unsigned char *bitmap, unsigned int n);

/**
 * Initializes a new inode in the next free spot.
 * 
 * Returns -1 on failure and errno is set.
 */
unsigned int new_inode(unsigned char *disk);

/**
 * Initializes a new block in the next free spot.
 * 
 * Returns -1 on failure and errno is set.
 */
unsigned int new_block(unsigned char *disk);

/**
 * Marks an inode as unused.
 */
void rm_inode(unsigned char *disk, unsigned int inode);

/**
 * Marks an inode as unused.
 */
void rm_block(unsigned char *disk, unsigned int block);

/**
 * Initializes a new inode as a directory in the next free spot.
 * 
 * Returns -1 on failure and errno is set.
 */
unsigned int new_inode_dir(unsigned char *disk);

/**
 * Initializes a new inode as a file in the next free spot.
 * 
 * Returns -1 on failure and errno is set.
 */
unsigned int new_inode_file(unsigned char *disk);

/**
 * Initializes a new inode as a link in the next free spot.
 * 
 * Returns -1 on failure and errno is set.
 */
unsigned int new_inode_link(unsigned char *disk);

/**
 * Inserts a new directory entry into a directory inode with a given name pointing to a given inode.
 * 
 * Returns NULL on failure and errno is set.
 */
struct ext2_dir_entry *new_dir_entry(unsigned char *disk, unsigned int parent_inode, unsigned int child_inode, char *name, unsigned char file_type);

/**
 * Removes a directory entry given its name from the directory inode.
 * 
 * Returns NULL on failure and errno is set.
 */
struct ext2_dir_entry *rm_dir_entry(unsigned char *disk, unsigned int parent_inode, char *name);
void rm_dir_entry_helper(unsigned char *disk, unsigned int inode, unsigned int block, void *arg);

/**
 * Reads a file into memory and returns a pointer to the beginning of the file in memory.
 */
char *read_to_memory(char *path);

/**
 * Returns a new path with path1 and path2 joined with a '/'
 */
char *path_join(char *path1, char *path2);

/**
 * Creates (strlen(source) / EXT2_BLOCK_SIZE) blocks and writes the source string into the blocks.
 * 
 * Returns NULL on failute and errno is set.
 */
struct ext2_dir_entry *write_string_to_blocks(unsigned char *disk, struct ext2_dir_entry *dir_entry, char *source);

/**
 * Increments dir_entry's rec_len with the next directory entry's rec_len.
 * 
 * Assumes that dir_entry is not the last directory entry.
 */
struct ext2_dir_entry *dir_entry_rm_next(struct ext2_dir_entry *dir_entry);

/**
 * Makes the dir_entry a blank record.
 * Returns the dir_entry after it was changed.
 */
struct ext2_dir_entry *dir_entry_to_blank(struct ext2_dir_entry *dir_entry);


/**
 * Returns the absolute difference between two unsigned ints.
 */
unsigned int unsigned_abs_diff(unsigned int x, unsigned int y);


/**
 * Returns true if name == ""."" or name == ""..""
 */
bool is_dot_or_dot_dot(char *name, unsigned char name_len);
