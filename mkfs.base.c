#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <linux/fs.h>
#include <string.h>
#include "base.h"
#include "bitops.h"

#define Inode (((struct base_inode *) inode_buffer) - 1)
#define unmark_zone(x) (clrbit(zone_map,(x)-get_first_zone()+1))
#define unmark_inode(x) (clrbit(inode_map,(x)))
#define mark_inode(x) (setbit(inode_map,(x)))
#define zone_in_use(x) (isset(zone_map,(x)-get_first_zone()+1) != 0)
# define mkfs_minix_time(x) time(x) 
//arrumar esse time


static unsigned short good_blocks_table[MAX_GOOD_BLOCKS];

static char root_block[BASE_BLOCK_SIZE];
static char boot_block_buffer[512];

static char boot_block_buffer[512];
static char *inode_buffer = NULL;
static char *inode_map;
static char *zone_map;

char *super_block_buffer;

struct fs_control{
     char *device_name; //  partition's name
     int device_fd;
     unsigned long long fs_blocks;
     int fs_magic;
     int fs_bad_blocks;
     int fs_used_blocks;
     size_t fs_dirsize;
     unsigned long fs_inodes;
};

static inline int write_all(int fd, const void *buf, size_t count)
{
    while (count) 
    {
      ssize_t tmp;
      errno = 0;
      tmp = write(fd, buf, count);
      if (tmp > 0) 
      {
	    count -= tmp;
	    if (count)
	    buf = (void *) ((char *) buf + tmp);
        } else if (errno != EINTR && errno != EAGAIN)
                return -1;
        if (errno == EAGAIN)
		xusleep(250000);
    }
   return 0;
}	

static void write_block(const struct fs_control *flc, int blk, char * buffer) {
    if (blk * BASE_BLOCK_SIZE != lseek(flc->device_fd, blk * BASE_BLOCK_SIZE, SEEK_SET))
     {	    
      printf("%s: seek failed in write_block", flc->device_name);
      exit(1);
     }

    if (write_all(flc->device_fd, buffer, BASE_BLOCK_SIZE))
    {
	printf("%s: write failed in write_block", flc->device_name);
        exit(1); 
    }
}


static int get_free_block(struct fs_control *flc)
{
    unsigned int blk;
    unsigned int zones = Super.s_nzones;
    unsigned int first_zone = get_first_zone();
    if (flc->fs_used_blocks + 1 >= MAX_GOOD_BLOCKS)
    {
	printf("%s: too many bad blocks", flc->device_name);
	exit(1);
    }
     if (flc->fs_used_blocks)
     blk = good_blocks_table[flc->fs_used_blocks - 1] + 1;
     else
     blk = first_zone;
     while (blk < zones && zone_in_use(blk))
     blk++;
    if (blk >= zones)
    {
     printf("%s: not enough good blocks", flc->device_name);
      exit(0);
    } 
     good_blocks_table[flc->fs_used_blocks] = blk;
     flc->fs_used_blocks++;
      return blk;

}	
static void make_bad_inode(struct fs_control *flc)
{


}	

static void make_root_inode(struct fs_control *flc)
{
	struct base_inode * inode = &Inode[BASE_ROOT_INO];
   char *tmp = root_block;	
   *(uint16_t *) tmp = 1;
    strcpy(tmp + 2, ".");
    tmp += flc->fs_dirsize;
    *(uint16_t *) tmp = 1;
    strcpy(tmp + 2, "..");
    tmp += flc->fs_dirsize;
    *(uint16_t *) tmp = 2;
    strcpy(tmp + 2, ".badblocks");
   mark_inode(BASE_ROOT_INO);
   inode->i_zone[0] = get_free_block(flc);
   inode->i_nlinks = 2;
    inode->i_time = mkfs_minix_time(NULL);
   if (flc->fs_bad_blocks)
     inode->i_size = 3 * flc->fs_dirsize;
   else {
        root_block[2 * flc->fs_dirsize] = '\0';
	root_block[2 * flc->fs_dirsize + 1] = '\0';
	inode->i_size = 2 * flc->fs_dirsize;
   }						           

   inode->i_mode = S_IFDIR + 0755;
   inode->i_uid = getuid();
   if (inode->i_uid)
   inode->i_gid = getgid();
   write_block(flc, inode->i_zone[0],root_block);
}



 static void setup_tables(const struct fs_control *flc) 
{
   unsigned long zones,inodes, zmaps, imaps,i;

   super_block_buffer =  calloc(1,BASE_BLOCK_SIZE);
   if (super_block_buffer == NULL) exit(1);
   Super.s_magic = flc->fs_magic;
   Super.s_log_zone_size = 0;
   Super.s_max_size = (7+512+512*512)*1024;
   Super.s_nzones  = zones = flc->fs_blocks;
   
   if (flc->fs_inodes == 0)
      if (2048 * 1024 < flc->fs_blocks)  
        inodes = flc->fs_blocks / 16;
      else if (512 * 1024 < flc->fs_blocks)   /* 0.5GB */
	   inodes = flc->fs_blocks / 8;   
    else
     inodes = flc->fs_blocks / 3;

    inodes = ((inodes + BASE_INODES_PER_BLOCK - 1) &
	 ~(BASE_INODES_PER_BLOCK - 1));

    Super.s_ninodes = inodes;
    if (inodes > BASE_MAX_INODES)
	inodes = BASE_MAX_INODES;
    Super.s_imap_blocks = UPPER(inodes + 1, BITS_PER_BLOCK); 
    Super.s_zmap_blocks = UPPER(flc->fs_blocks - (1 + Super.s_imap_blocks + 
			    UPPER(Super.s_ninodes, BASE_INODES_PER_BLOCK)),BITS_PER_BLOCK + 1);
    Super.s_firstdatazone = first_zone_data();
  
     imaps = Super.s_imap_blocks;
     zmaps = Super.s_zmap_blocks;
     inode_map = malloc(imaps * BASE_BLOCK_SIZE);
     if(inode_map == NULL) exit(1);
     zone_map = malloc(zmaps * BASE_BLOCK_SIZE);
     if(zone_map == NULL) exit(1);
     memset(inode_map,0xff,imaps * BASE_BLOCK_SIZE);
     memset(zone_map,0xff,zmaps * BASE_BLOCK_SIZE);
     for (i = get_first_zone() ; i<zones ; i++)
    
	  unmark_zone(i);	     
     for (i = BASE_ROOT_INO ; i<=inodes; i++)
     unmark_inode(i);

     inode_buffer = malloc(get_inode_buffer_size());
     memset(inode_buffer,0, get_inode_buffer_size());

}	

static void determine_device_blocks(struct fs_control *flc)
{
   unsigned long long dev_blocks;    
   int sector_size;
#ifdef BLKSSZGET
     ioctl(flc->device_fd, BLKSSZGET, &sector_size);
#else
    sector_size = DEFAULT_SECTOR_SIZE;
#endif    

    ioctl(flc->device_fd, BLKGETSIZE64,flc->fs_blocks); // TESTAR TAMANHO DO FLC->FS_BLOCKS

}	


int main(int argc, char **argv)
{
    struct fs_control  flc; //file system control
    flc.fs_magic = BASE_SUPER_MAGIC;
    flc.device_name = argv[1];
    flc.device_fd = open(flc.device_name, O_RDWR | O_EXCL);
//TODO
//fazer uma funcao que se o cara nao entre com a particao ele de uma mensagem de erro
//uma funcao pra  ver se a particao passada é um bloco valido
//uma funcao pra ver se a particao nao esta montada.

    if (flc.device_fd < 0)
    {
        perror(flc.device_name);
        return 0;	
    }

determine_device_blocks(&flc);
 setup_tables(&flc);
  make_root_inode(&flc);
  //make_bad_inode(&ctl);




  return 0;
}


