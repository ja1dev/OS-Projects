/*
 *  Copyright (C) 2022 CS416/518 Rutgers CS
 *	RU File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

//for some reason it doesnt recognize the disk size
#define DISK_SIZE	32*1024*1024
// Declare your in-memory data structures here
struct superblock superblock;
bitmap_t inode_bitmap;
bitmap_t dblock_bitmap;
int number_of_blocks = 0;
int disk_num = -1;
int inum = MAX_INUM/8;
/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	void * temp = malloc(BLOCK_SIZE);
	bio_read(1,temp);
	memcpy(inode_bitmap, temp, inum);
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i < MAX_INUM; i++){
		if (get_bitmap(inode_bitmap, i)==0){
			// Step 3: Update inode bitmap and write to disk 
			set_bitmap(inode_bitmap, i);
			bio_write(1,inode_bitmap);
			return i;
		}
	}
	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	bio_read(2, dblock_bitmap);
	for(int i = 0; i<(number_of_blocks); i++){
		// Step 2: Traverse data block bitmap to find an available slot
		if (get_bitmap(dblock_bitmap, i) == 0){
			set_bitmap(dblock_bitmap,i);
			bio_write(2, dblock_bitmap);
			return (67 + i);
		}
	}
	

	// Step 3: Update data block bitmap and write to disk 

	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
	int blknum = 3 + (ino / (BLOCK_SIZE / sizeof(struct inode)));
  // Step 2: Get offset of the inode in the inode on-disk block
	struct inode *inode_block = (struct inode *) malloc(BLOCK_SIZE);
  // Step 3: Read the block from disk and then copy into inode structure
	bio_read(blknum, inode_block);
	memcpy(inode, &inode_block[ino%16], sizeof(struct inode));
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int blknum = 3 + (ino / 16);
	// Step 2: Get the offset in the block where this inode resides on disk
	struct inode *inode_block = (struct inode *) malloc(BLOCK_SIZE);
	// Step 3: Write inode to disk 
	bio_read(blknum, inode_block);
	memcpy(&inode_block[ino%16], inode, sizeof(struct inode));
	bio_write(blknum, inode_block);
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* curr_inode = (struct inode *)malloc(sizeof(struct inode));
	readi(ino, curr_inode);
  // Step 2: Get data block of current directory from inode
	struct dirent* curr_dblock = (struct dirent *)malloc(BLOCK_SIZE);
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
	int blknum = 0;
	int i = 0;
	for (blknum = 0; blknum < 16; blknum++){
		if (curr_inode->direct_ptr[blknum] == 0){
			continue;
		}
		bio_read(curr_inode->direct_ptr[blknum], curr_dblock);
		for (i = 0; i < 16; i++){
			if (curr_dblock[i].valid == 1){
				if (strcmp(curr_dblock[i].name, fname) == 0){
					memcpy(dirent, &curr_dblock[i], sizeof(struct dirent));
					return curr_inode->direct_ptr[blknum];
				}
			}
		}
	}
	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	struct dirent* curr_dblock = (struct dirent *)malloc(BLOCK_SIZE);
	struct dirent* dirent = malloc(sizeof(struct dirent));
	int blknum = 0;
	int i = 0;

	// Step 2: Check if fname (directory name) is already used in other entries
	if (dir_find(dir_inode.ino, fname, name_len, dirent)!=0){
		return 0;
	}
	dirent->ino = f_ino;
	dirent->valid = 1;
	strncpy(dirent->name, fname, sizeof(dirent->name));
	dirent->name[name_len] = '\0';
	for (blknum = 0; blknum < 16; blknum++){
		if (dir_inode.direct_ptr[blknum] == 0) {
			dir_inode.direct_ptr[blknum] = get_avail_blkno();
			writei(dir_inode.ino, &dir_inode);
			bio_write(dir_inode.direct_ptr[blknum], dirent);
			return 0;
		}
		bio_read(dir_inode.direct_ptr[blknum], curr_dblock);
		for (i = 0; i < 16;i++) {
			if (curr_dblock[i].valid == 0) {
				memcpy(&curr_dblock[i], dirent, sizeof(struct dirent));
				bio_write(dir_inode.direct_ptr[blknum], curr_dblock);
				dir_inode.link++;
				writei(dir_inode.ino, &dir_inode);
				return 0;
			}
		}
	}
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	
	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	struct dirent* curr_dblock = (struct dirent *)malloc(BLOCK_SIZE);
	struct dirent* dirent = malloc(sizeof(struct dirent));
	int i = 0;

	int block = dir_find(dir_inode.ino, fname, name_len, dirent);
	// Step 2: Check if fname exist
	if (block!=0) {
		curr_dblock = malloc(BLOCK_SIZE);
		bio_read(block, curr_dblock);
		for (i = 0; i < 16; i++) {
			// Step 3: If exist, then remove it from dir_inode's data block and write to disk
			if (curr_dblock[i].valid == 1) {
				if (strcmp(curr_dblock[i].name, fname) == 0) {
					curr_dblock[i].valid = 0;
					bio_write(block, curr_dblock);
					free(dirent);
					free(curr_dblock);
					return 0;
				}
			}
		}
	}
	free(dirent);
	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	if (strcmp(path, "/") == 0) {
		readi(0, inode);
		return 0;
	}
	//use root
	struct inode* root = (struct inode*)malloc(sizeof(struct inode));
	readi(ino, root);

	//start with the first tok
	char* start = strtok(strdup(path), "/");
	char* next = strdup(start);

	if (start == NULL) {
		return -ENOENT;
	}
	int curr = root->ino;
	struct dirent* next_dirent = malloc(sizeof(struct dirent));

	//iterative solution
	while (next != NULL) {
		if (dir_find(curr, next, 0, next_dirent) == 0) {
			return -ENOENT;
		}
		curr = next_dirent->ino;
		next = strtok(NULL, "/");
	}
	readi(curr, inode);

	return 0;

}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	// write superblock information
	struct superblock* superblock = (struct superblock*)malloc(BLOCK_SIZE);
	superblock->magic_num = MAGIC_NUM;
	superblock->max_inum = MAX_INUM;
	superblock->max_dnum = MAX_DNUM;

	superblock->i_bitmap_blk = 1;
	superblock->d_bitmap_blk = 2;
	superblock->i_start_blk = 3;
	superblock->d_start_blk = 67;
	bio_write(0, superblock);

	number_of_blocks = (DISK_SIZE/BLOCK_SIZE) - 67;
	// initialize inode bitmap
	inode_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	memset(inode_bitmap, 0, BLOCK_SIZE);
	// initialize data block bitmap
	dblock_bitmap =(bitmap_t)malloc(BLOCK_SIZE); 
	memset(dblock_bitmap, 0, BLOCK_SIZE);
	// update bitmap information for root directory
	set_bitmap(inode_bitmap, 0);
	set_bitmap(dblock_bitmap, 0);
	bio_write(1, inode_bitmap);
	bio_write(2, dblock_bitmap);
	// update inode for root directory
	struct inode root;
	root.ino = 0;
	root.size = BLOCK_SIZE;
	root.valid = 1;
	root.type = 0;
	root.link = 2;
	memset(root.direct_ptr, 0, 16*sizeof(int));
	memset(root.indirect_ptr, 0, 8*sizeof(int));
	writei(0, &root);

	//this var determined if disk file found or not;
	disk_num = 0;
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	if (disk_num < 0){
		rufs_mkfs();
	}
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	else{
		bio_read(0, &superblock);
		bio_read(0, &inode_bitmap);
		bio_read(0, &dblock_bitmap);
	}
	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(inode_bitmap);
	free(dblock_bitmap);
	// Step 2: Close diskfile
	disk_num = -1; //set this back to glob val
	dev_close();
}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(path, 0, curr_inode) == -ENOENT){
		return -ENOENT;
	}
	// Step 2: fill attribute of file into stbuf from inode
	if (curr_inode->type == 0){
		stbuf->st_mode = S_IFDIR;
	}
	else{
		stbuf->st_mode = S_IFREG;
	}
	stbuf->st_nlink  = curr_inode->link;

	stbuf->st_ino    = curr_inode->ino;
	stbuf->st_size   = curr_inode->size;
	stbuf->st_gid    = getgid();
	stbuf->st_uid    = getuid();
	stbuf->st_mtime  = time(NULL);
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(path, 0, curr_inode) == 0){
		return 0;
	}
	// Step 2: If not find, return -1
	else {
		return -1;
	}
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(path, 0, curr_inode) == 0){
		return -ENOENT;
	}
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	struct dirent* curr_dblock = (struct dirent*)malloc(BLOCK_SIZE);
	for (int blknum = 0; blknum < 16; blknum++){
		if (curr_inode->direct_ptr[blknum] == 0){
			continue;
		}
		bio_read(curr_inode->direct_ptr[blknum], curr_dblock);
		for (int i = 0; i < 16; i++){
			if (curr_dblock[i].valid == 1){
				filler(buffer, curr_dblock[i].name, NULL, 0);
			}
		}
	}
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * dir_path = dirname(strdup(path));
	char * dir_name = basename(strdup(path));
	
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(dir_path, 0, curr_inode) == -ENOENT){
		perror("dictionary already exists");
		return -ENOENT;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int ino = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*curr_inode, ino, dir_name, strlen(dir_name));
		
	// Step 5: Update inode for target directory
	struct inode* target = malloc(sizeof(struct inode));
	target->valid = 1;
	target->ino = ino;
	target->size = BLOCK_SIZE;
	target->link = 2;
	target->type = 0;
	memset(target->direct_ptr, 0, sizeof(int)*16);
	memset(target->indirect_ptr, 0, sizeof(int)*8);
	// Step 6: Call writei() to write inode to disk
	writei(target->ino, target);
	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * dir_path = dirname(strdup(path));
	char * dir_name = basename(strdup(path));
	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	get_node_by_path(dir_name, 0, curr_inode);

	// Step 3: Clear data block bitmap of target director
	// Step 4: Clear inode bitmap and its data block

	bio_read(1,inode_bitmap);
	unset_bitmap(inode_bitmap, curr_inode->ino);
	bio_write(1, inode_bitmap);

	bio_read(2, dblock_bitmap);
	for (int i = 0; i < 16; i++){
		if (curr_inode->direct_ptr[i] != 0){
			unset_bitmap(dblock_bitmap, curr_inode->direct_ptr[i]-67);
		}
	}
	bio_write(2, dblock_bitmap);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dir_path, 0, curr_inode);
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	dir_remove(*curr_inode, dir_name, 0);
	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char * dir_path = dirname(strdup(path));
	char * f_name = basename(strdup(path));
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(dir_path, 0, curr_inode) == -ENOENT){
		perror("dictionary already exists");
		return -ENOENT;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int ino = get_avail_ino();
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(*curr_inode, ino, f_name, strlen(f_name));
	// Step 5: Update inode for target file
	struct inode* target = malloc(sizeof(struct inode));
	target->valid = 1;
	target->ino = ino;
	target->size = 0;
	target->link = 1;
	target->type = 1;
	memset(target->direct_ptr, 0, sizeof(int)*16);
	memset(target->indirect_ptr, 0, sizeof(int)*8);

	// Step 6: Call writei() to write inode to disk
	writei(target->ino, target);
	readi(target->ino, curr_inode);
	
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(path, 0, curr_inode) == 0){
		return 0;
	}
	// Step 2: If not find, return -1

	return -1;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	get_node_by_path(path, 0, curr_inode);
	// Step 2: Based on size and offset, read its data blocks from disk
	int block_index = offset / BLOCK_SIZE;
	int block_offset = (offset+size)/BLOCK_SIZE;
	int numblocks = block_offset-block_index;
	int blocks_ptr =0;
	void* buff = (void*)malloc(BLOCK_SIZE);
	int* block_indir = (int *)malloc(BLOCK_SIZE);

	// Step 3: copy the correct amount of data from offset to buffer
	//lets walk through
	for(int i = block_index; i < block_index + numblocks; i++){
		if (i >=16){
			bio_read(curr_inode->indirect_ptr[i/1024], block_indir);
			bio_read(block_indir[i % 1024], buff);
			memcpy(buffer + BLOCK_SIZE*blocks_ptr, buff, BLOCK_SIZE);
		}
		else{
			bio_read(curr_inode->direct_ptr[i], buff);
			memcpy(buffer + BLOCK_SIZE*blocks_ptr, buff, BLOCK_SIZE);
			blocks_ptr++;
		}
	}
	// Note: this function should return the amount of bytes you copied to buffer
	//return 0;
	return size;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	get_node_by_path(path, 0, curr_inode);
	// Step 2: Based on size and offset, read its data blocks from disk
	int block_index= offset / BLOCK_SIZE;
	void* buff = (void*)malloc(BLOCK_SIZE);
	int* block_indir = (int *)malloc(BLOCK_SIZE);
	if (block_index < 16){
		if (curr_inode->direct_ptr[block_index] == 0){
			curr_inode->direct_ptr[block_index] = get_avail_blkno();
			//step
		}
		//copy
		bio_read(curr_inode->direct_ptr[block_index], buff);
		memcpy(buff, buffer, BLOCK_SIZE);
		bio_write(curr_inode->direct_ptr[block_index], buff);
		//increase size and write over
		curr_inode->size += size;
		writei(curr_inode->ino, curr_inode);
		return size;
	}
	// Step 3: Write the correct amount of data from offset to disk
	block_index = (block_index-16) % 1024;
	int new_block_index = block_index / 1024;
	if (curr_inode->indirect_ptr[new_block_index] == 0){
		curr_inode->indirect_ptr[new_block_index] = get_avail_blkno();
		memset(block_indir, 0, BLOCK_SIZE);
	}
	else{
		bio_read(curr_inode->indirect_ptr[new_block_index], block_indir);
	}

	if(block_indir[block_index] == 0){
		block_indir[block_index]= get_avail_blkno();
		bio_write(curr_inode->indirect_ptr[new_block_index], block_indir);
	}
	bio_read(block_indir[block_index], buff);
	memcpy(buff, buffer, BLOCK_SIZE);
	bio_write(block_indir[block_index], buff);

	curr_inode->size += size;
	// Step 4: Update the inode info and write it to disk
	writei(curr_inode->ino, curr_inode);
	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char * dir_path = dirname(strdup(path));
	char * f_name = basename(strdup(path));
	// Step 2: Call get_node_by_path() to get inode of target file
	struct inode* curr_inode = (struct inode*)malloc(sizeof(struct inode));
	get_node_by_path(f_name, 0, curr_inode);
	// Step 3: Clear data block bitmap of target file
	// Step 4: Clear inode bitmap and its data block
	bio_read(1,inode_bitmap);
	unset_bitmap(inode_bitmap, curr_inode->ino);
	bio_write(1, inode_bitmap);

	bio_read(2, dblock_bitmap);
	for (int i = 0; i < 16; i++){
		if (curr_inode->direct_ptr[i] != 0){
			unset_bitmap(dblock_bitmap, curr_inode->direct_ptr[i]-67);
		}
	}
	//write the cleared map
	bio_write(2, dblock_bitmap);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	get_node_by_path(dir_path, 0, curr_inode);
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	dir_remove(*curr_inode, f_name, 0);
	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

