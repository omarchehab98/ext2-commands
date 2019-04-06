/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"

unsigned char *disk;

void usage(char *program) {
	fprintf(stderr, "usage: %s <image file name> <path>\n", program);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		usage(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	if (!is_abs_path(argv[2])) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(EINVAL));
		exit(EXIT_FAILURE);
	}

	disk = load_disk(argv[1]);

	trim_trailing_slash(argv[2]);
	char *path = get_filepath(argv[2]);
	char *name = get_filename(argv[2]);

	unsigned int parent_inode = inode_by_filepath(disk, path);
	if (parent_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), path, strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	struct ext2_inode *parent_inode_entry = inode_from_index(disk, parent_inode);
	if (!S_ISDIR(parent_inode_entry->i_mode)) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), path, strerror(ENOTDIR));
		exit(EXIT_FAILURE);
	}
	
	unsigned int file_inode = inode_by_filepath(disk, argv[2]);
	if (file_inode != -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(EEXIST));
		exit(EXIT_FAILURE);
	}

	unsigned int child_inode = new_inode_dir(disk);
	if (child_inode == -1) {
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	struct ext2_group_desc *bg = DISK_GROUP_DESC(disk);
	bg->bg_used_dirs_count++;
	
	struct ext2_dir_entry* dir = new_dir_entry(disk, parent_inode, child_inode, name, EXT2_FT_DIR);
	if (dir == NULL) {
		// TODO: delete child_inode
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}

	struct ext2_dir_entry* dir_dot = new_dir_entry(disk, child_inode, child_inode, ".", EXT2_FT_DIR);
	if (dir_dot == NULL) {
		// TODO: delete child_inode, dir
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	struct ext2_dir_entry* dir_dot_dot = new_dir_entry(disk, child_inode, parent_inode, "..", EXT2_FT_DIR);
	if (dir_dot_dot == NULL) {
		// TODO: delete child_inode, dir, dir_dot
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}

	return 0;
}
