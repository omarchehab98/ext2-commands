/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ext2.h"


unsigned char *disk;


char bit_to_char(unsigned char bit) {
	return (char) (bit + 48);
}


char *bitmap_to_string(unsigned char *cs, int n) {
	int i, j;
	// (n bits for 1s and 0s) + (n / 8 bits for spaces) + (1 bit for null)
	char *str = malloc((n + (n / 8) + 1) * sizeof(char));
	if (str == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < n / 8; i++) {
		for (j = 0; j < 8; j++) {
			str[i * 9 + j] = bit_to_char(1 & cs[i] >> j);
		}
		str[i * 9 + j] = ' ';
	}
	str[i * 9 + j] = '\0';
	return str;
}


char inode_filemode_to_string(unsigned short i_mode) {
	if (S_ISREG(i_mode)) {
		return 'f';
	} else if (S_ISDIR(i_mode)) {
		return 'd';
	} else if (S_ISLNK(i_mode)) {
		return 'l';
	} else {
		return 'u';
	}
}


char dir_entry_file_type_to_string(unsigned char file_type) {
	if (EXT2_FT_REG_FILE == file_type) {
		return 'f';
	} else if (EXT2_FT_DIR == file_type) {
		return 'd';
	} else if (EXT2_FT_SYMLINK == file_type) {
		return 'l';
	} else {
		return 'u';
	}
}


bool inode_should_skip(unsigned int inode) {
	return inode != EXT2_ROOT_INO && inode < 12;
}


bool inode_block_foreach_helper(unsigned int inode, unsigned int *i_block, unsigned int blocks, unsigned int indirection, void (*callback)(unsigned short, unsigned int, void *arg), void *arg) {
	for (int i = 0; i < blocks; i++) {
		unsigned int block = i_block[i];
		if (block) {
			if (indirection) {
				(*callback)(inode, block, arg);
				unsigned int *ib1 = (unsigned int *)(disk + EXT2_BLOCK_SIZE * block);
				inode_block_foreach_helper(inode, ib1, 256, indirection - 1, callback, arg);
			} else {
				(*callback)(inode, block, arg);
			}
		} else {
			return false;
		}
	}
	return true;
}


void inode_block_foreach(unsigned int inode, void (*callback)(unsigned short, unsigned int, void *), void *arg) {
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	struct ext2_inode *inode_tbl = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * bg->bg_inode_table);

	struct ext2_inode in = inode_tbl[inode];
	inode_block_foreach_helper(inode, in.i_block, 12, 0, callback, arg)
		&& inode_block_foreach_helper(inode, in.i_block + 12, 1, 1, callback, arg)
		&& inode_block_foreach_helper(inode, in.i_block + 13, 1, 2, callback, arg)
		&& inode_block_foreach_helper(inode, in.i_block + 14, 1, 3, callback, arg);
}


void print_inode_block(unsigned short inode, unsigned int iblock, void *arg) {
	printf("%d ", iblock);
}


void print_inode_blocks(unsigned int i) {
	inode_block_foreach(i, &print_inode_block, NULL);
}


void inode_foreach(void (*callback)(unsigned int, void *), void *arg) {
	struct ext2_super_block *s = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char *inode_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * bg->bg_inode_bitmap);

	for (unsigned int i = 0; i < s->s_inodes_count / 8; i++) {
		for (unsigned int j = 0; j < 8; j++) {
			unsigned int index = i * 8 + j + 1;
			if (1 & inode_bitmap[i] >> j && !inode_should_skip(index)) {
				(*callback)(index - 1, arg);
			}
		}
	}
}


void print_inode(unsigned int i, void *arg) {
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	struct ext2_inode *inode_tbl = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * bg->bg_inode_table);

	struct ext2_inode inode = inode_tbl[i];
	printf("[%d] type: %c size: %d links: %d blocks: %d\n", i + 1, inode_filemode_to_string(inode.i_mode), inode.i_size, inode.i_links_count, inode.i_blocks);
	printf("[%d] Blocks:  ", i + 1);
	print_inode_blocks(i);
	printf("\n");
}


void print_directory_helper(unsigned short inode, unsigned int iblock, void *arg) {
	struct ext2_dir_entry *e;
	unsigned short rec_total;
	unsigned char *ep = disk + EXT2_BLOCK_SIZE * iblock;

	printf("   DIR BLOCK NUM: %d (for inode %d)\n", iblock, inode + 1);
	for (rec_total = 0; rec_total < EXT2_BLOCK_SIZE; rec_total += e->rec_len, ep += e->rec_len) {
		e = (struct ext2_dir_entry *)(ep);

		char file_type = dir_entry_file_type_to_string(e->file_type);

		printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n", e->inode, e->rec_len, e->name_len, file_type, e->name_len, e->name);
	}
}


void print_directory(unsigned int i, void *arg) {
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	struct ext2_inode *inode_tbl = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * bg->bg_inode_table);

	struct ext2_inode inode = inode_tbl[i];
	if (S_ISDIR(inode.i_mode)) {
		inode_block_foreach(i, &print_directory_helper, arg);
	}
}


int main(int argc, char **argv) {

	if(argc != 2) {
		fprintf(stderr, "usage: %s <image file name>\n", argv[0]);
		exit(1);
	}
	int fd = open(argv[1], O_RDWR);
	if(fd == -1) {
		perror("open");
		exit(1);
	}

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	struct ext2_super_block *s = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	printf("Inodes: %d\n", s->s_inodes_count);
	printf("Blocks: %d\n", s->s_blocks_count);

	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	printf("Block group:\n");
	printf("    block bitmap: %d\n", bg->bg_block_bitmap);
	printf("    inode bitmap: %d\n", bg->bg_inode_bitmap);
	printf("    inode table: %d\n", bg->bg_inode_table);
	printf("    free blocks: %d\n", bg->bg_free_blocks_count);
	printf("    free inodes: %d\n", bg->bg_free_inodes_count);
	printf("    used_dirs: %d\n", bg->bg_used_dirs_count);

	unsigned char *block_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * bg->bg_block_bitmap);
	printf("Block bitmap: %s\n", bitmap_to_string(block_bitmap, s->s_blocks_count));

	unsigned char *inode_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * bg->bg_inode_bitmap);
	printf("Inode bitmap: %s\n", bitmap_to_string(inode_bitmap, s->s_inodes_count));
	
	printf("\n");

	printf("Inodes:\n");
	
	inode_foreach(&print_inode, NULL);
	printf("\n");

	printf("Directory Blocks:\n");
	inode_foreach(&print_directory, NULL);

	return 0;
}