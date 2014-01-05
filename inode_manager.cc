#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

#define CEIL(a, b) (((a) / (b)) + ((a) % (b) ? 1 : 0))

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  char buf[BLOCK_SIZE];
  for (blockid_t i = IBLOCK(sb.ninodes- 1, sb.nblocks) + 1; i < sb.nblocks; i++) {
	  read_block(BBLOCK(i), buf);
	  blockid_t t = i - BBLOCK(i);
	  if (!(buf[t >> 3] & (1 << (t % 8)))) {
		  buf[t >> 3] |= (1 << (t % 8));
		  write_block(BBLOCK(i), buf);
		  return i;
	  }
  }



  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  blockid_t i, t; 
  i = BBLOCK(id);
  t = id - i;
  read_block(i, buf);
  buf[t >> 3] &= ~(((uint8_t) 1) << (t % 8));
  write_block(i, buf);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  char buf[BLOCK_SIZE];
  struct inode *ino;
  time_t cur = time(NULL);

  for (blockid_t i = 0; i < CEIL(bm->sb.ninodes, IPB); i++) {
	  bm->read_block(i + IBLOCK(0, bm->sb.nblocks), buf);
	  for (blockid_t j = 0; j < IPB; j++) {
		  //skip the inode[0]
		  if (!(i || j))
			  continue;
		  if (j + i * IPB >= INODE_NUM)
			  return 0;
		  ino = ((struct inode *) buf) + j;
		  if (ino->type == 0) {
			  ino->type = type;
			  ino->size = 0;
			  ino->mtime = ino->ctime = cur;
			  for (int k = 0; k < NDIRECT + 1; k++)
				  ino->blocks[k] = 0;
			  bm->write_block(i + IBLOCK(0, bm->sb.nblocks), buf);
			  return j + i * IPB;
		  }
	  }
  }
  return 0; //TODO originally it's 1
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

	struct inode *ino = get_inode(inum);
	if (!ino)
		return;
	time_t cur = time(NULL);

	if (ino->type) {
		ino->type = 0;
		ino->mtime = ino->ctime = cur;
		put_inode(inum, ino);
	}
	free(ino);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define BLK(ino,ind,i) \
	((i) < NDIRECT ? (ino)->blocks[i] : *(((blockid_t *) (ind)) + ((i) - NDIRECT)))

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
	 *
   */
	struct inode *ino_disk = get_inode(inum);
	if (!ino_disk)
		return;
	if (ino_disk->type == 0) {
		free(ino_disk);
		*buf_out = NULL;
		*size = 0;
		return;
	}

	char buf[BLOCK_SIZE], ind_buf[BLOCK_SIZE];
	time_t cur = time(NULL);
	int bc = CEIL(ino_disk->size, BLOCK_SIZE);

	if (bc > NDIRECT)
		bm->read_block(ino_disk->blocks[NDIRECT], ind_buf);

	*size = ino_disk->size;
	if (!*size) {
		buf_out = NULL;
		free(ino_disk);
		return;
	}
	*buf_out = (char *) malloc(ino_disk->size * sizeof(char));
	
	int i;
	for (i = 0; i < bc - 1; i++) {
		bm->read_block(BLK(ino_disk, ind_buf, i), buf);
		memcpy((*buf_out) + i * BLOCK_SIZE, buf, BLOCK_SIZE);
	}
	bm->read_block(BLK(ino_disk, ind_buf, i), buf);
	memcpy((*buf_out) + i * BLOCK_SIZE, buf, ino_disk->size % BLOCK_SIZE ? ino_disk->size % BLOCK_SIZE : BLOCK_SIZE);

	ino_disk->atime = (uint32_t) cur;
	put_inode(inum, ino_disk);
	free(ino_disk);

	return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
	struct inode *ino_disk = get_inode(inum);
	if (!ino_disk)
		return;
	if (ino_disk->type == 0) {
		free(ino_disk);
		return;
	}

	char ind_buf[BLOCK_SIZE];

	time_t cur = time(NULL);

	int bc = CEIL(ino_disk->size, BLOCK_SIZE),
		bc_new = CEIL(size, BLOCK_SIZE);

	if (bc > NDIRECT)
		bm->read_block(ino_disk->blocks[NDIRECT], ind_buf);

	for (int i = 0; i < MIN(bc_new, bc); i++) {
		bm->write_block(BLK(ino_disk, ind_buf, i), buf + i * BLOCK_SIZE);
	}

	if (bc_new == bc) {
		ino_disk->mtime = ino_disk->ctime = cur;
		if ((uint32_t) size != ino_disk->size) {
			ino_disk->size = (uint32_t) size;
			ino_disk->ctime = cur;
		}
		put_inode(inum, ino_disk);
	} else if (bc_new < bc) {
		for (int i = bc_new; i < bc; i++) {
			bm->free_block(BLK(ino_disk, ind_buf, i));
			BLK(ino_disk, ind_buf, i) = 0;
		}
		if (bc > NDIRECT && bc_new <= NDIRECT) {
			bm->free_block(ino_disk->blocks[NDIRECT]);
			ino_disk->blocks[NDIRECT] = 0;
		}

		ino_disk->ctime = ino_disk->mtime = cur;
		ino_disk->size = (uint32_t) size;
		put_inode(inum, ino_disk);
	} else {
		if (bc <= NDIRECT && bc_new > NDIRECT) {
			ino_disk->blocks[NDIRECT] = bm->alloc_block();
			bzero(ind_buf, sizeof(ind_buf));
		}
		for (int i = bc; i < bc_new; i++) {
			BLK(ino_disk, ind_buf, i) = bm->alloc_block();
			bm->write_block(BLK(ino_disk, ind_buf, i), buf + i * BLOCK_SIZE);
		}

		if (ino_disk->blocks[NDIRECT])
			bm->write_block(ino_disk->blocks[NDIRECT], ind_buf);

		ino_disk->ctime = ino_disk->mtime = cur;
		ino_disk->size = (uint32_t) size;
		put_inode(inum, ino_disk);
	}
	free(ino_disk);
	return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
	char buf[BLOCK_SIZE];
	struct inode *ino_disk;

	blockid_t blockid = IBLOCK(inum, bm->sb.nblocks);
	bm->read_block(blockid, buf);
	ino_disk = ((struct inode *) buf) + inum % IPB;

	a.type = ino_disk->type;
	a.size = ino_disk->size;
	a.atime = ino_disk->atime;
	a.ctime = ino_disk->ctime;
	a.mtime = ino_disk->mtime;

  
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */
	struct inode *ino = get_inode(inum);
	char ind_buf[BLOCK_SIZE];
	if (!ino)
		return;
	if (!ino->type) {
		free(ino);
		return;
	}
	
	int bc = CEIL(ino->size, BLOCK_SIZE);
	if (bc > NDIRECT) {
		bm->read_block(ino->blocks[NDIRECT], ind_buf);
		bm->free_block(ino->blocks[NDIRECT]);
	}
	
	for (int i = 0; i < bc; i++) {
		bm->free_block(BLK(ino, ind_buf, i));
	}

	free(ino);
	free_inode(inum);

  return;
}
