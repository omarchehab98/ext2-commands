/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"

unsigned char *disk;

void usage(char *program) {
	fprintf(stderr, "usage: %s <image file name> <path to file>\n", program);
}


struct restore_dir_entry_data {
	char *name;
	unsigned char name_len;
	bool done;
	struct ext2_dir_entry *dir_entry;
};


void datablock_is_ok(unsigned char *disk, unsigned int inode, unsigned int block, void *arg) {
	bool *datablocksOk = (bool *) arg;

	unsigned char *bb = DISK_BLOCK_BITMAP(disk);

	if (is_bit_set_by_index(bb, block)) {
		*datablocksOk = false;
	}
}

void set_datablock_in_bitmap(unsigned char *disk, unsigned int inode, unsigned int block, void *arg) {
	struct ext2_super_block *s = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *bg = DISK_GROUP_DESC(disk);
	unsigned char *bb = DISK_BLOCK_BITMAP(disk);

	set_bit_by_index(bb, block);
	s->s_free_blocks_count--;
	bg->bg_free_blocks_count--;
}


void restore_dir_entry_helper(unsigned char *disk, struct ext2_dir_entry *dir_entry, void *arg) {
	struct restore_dir_entry_data *data = (struct restore_dir_entry_data *) arg;

	struct ext2_super_block *s = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *bg = DISK_GROUP_DESC(disk);
	unsigned char *ib = DISK_INODE_BITMAP(disk);

	unsigned int min_dir_entry_size = rec_len_boundary(sizeof(struct ext2_dir_entry) + 1);

	if (!data->done) {
		unsigned int dir_entry_size = rec_len_boundary(sizeof(struct ext2_dir_entry) + dir_entry->name_len);
		unsigned int padding = dir_entry->rec_len - dir_entry_size;

		while (min_dir_entry_size <= padding) {
			struct ext2_dir_entry *deleted_dir_entry = (struct ext2_dir_entry *)((void *) dir_entry + dir_entry_size);

			if (deleted_dir_entry->inode) {
				if (
					!is_bit_set_by_index(ib, deleted_dir_entry->inode - 1)
					&& data->name_len == deleted_dir_entry->name_len
					&& strncmp(data->name, deleted_dir_entry->name, deleted_dir_entry->name_len) == 0
				) {
					bool datablocksOk = true;

					inode_block_foreach(disk, deleted_dir_entry->inode - 1, &datablock_is_ok, &datablocksOk);

					if (datablocksOk) {
						struct ext2_inode *inode_entry = inode_from_index(disk, deleted_dir_entry->inode - 1);

						if (deleted_dir_entry->file_type == EXT2_FT_DIR) {
							fprintf(stderr, "%s: %s\n", data->name, strerror(EISDIR));
							exit(EXIT_FAILURE);
						}

						inode_entry->i_dtime = 0;
						inode_entry->i_links_count++;
						dir_entry->rec_len = dir_entry_size;
						deleted_dir_entry->rec_len = padding;

						data->dir_entry = deleted_dir_entry;

						set_bit_by_index(ib, deleted_dir_entry->inode - 1);
						inode_block_foreach(disk, deleted_dir_entry->inode - 1, &set_datablock_in_bitmap, NULL);

						s->s_free_inodes_count--;
						bg->bg_free_inodes_count--;

						data->done = 1;
					}
				}

				dir_entry = deleted_dir_entry;
				dir_entry_size = rec_len_boundary(sizeof(struct ext2_dir_entry) + dir_entry->name_len);
				padding -= rec_len_boundary(sizeof (struct ext2_dir_entry) + deleted_dir_entry->name_len);
			} else {
				padding = 0;
			}
		}

	}
}


struct ext2_dir_entry *restore_dir_entry(unsigned char *disk, unsigned int inode, char *name) {
	struct restore_dir_entry_data *data = malloc(sizeof (struct restore_dir_entry_data));
	if (data == NULL) {
		return NULL;
	}
	
	data->name = name;
	data->name_len = strlen(data->name);
	data->done = 0;
	data->dir_entry = NULL;
	
	directory_entry_foreach(disk, inode, &restore_dir_entry_helper, data);

	if (!data->done) {
		errno = ENOENT;
	}

	return data->dir_entry;
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

	char *path = get_filepath(argv[2]);
	char *name = get_filename(argv[2]);

	unsigned int file_inode = inode_by_filepath(disk, argv[2]);
	if (file_inode != -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(EEXIST));
		exit(EXIT_FAILURE);
	}

	unsigned int path_inode = inode_by_filepath(disk, path);
	if (path_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), path, strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	if (restore_dir_entry(disk, path_inode, name) == NULL) {
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	return 0;
}
