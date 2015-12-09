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

#define unmark_zone(x) (clrbit(zone_map,(x)-get_first_zone()+1))
#define unmark_inode(x) (clrbit(inode_map,(x)))
#define mark_inode(x) (setbit(inode_map,(x)))


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
     size_t fs_dirsize;
     unsigned long fs_inodes;
};

static void make_root_inode(struct fs_control *flc)
{
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




  return 0;
}


