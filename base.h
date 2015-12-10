#ifndef BASE_H
#define BASE_H
#include <stdint.h>
#include <time.h>

extern char *super_block_buffer;
#define Super (*(struct base_super_block *) super_block_buffer)
#define BASE_SUPER_MAGIC   0x138F
#define BASE_BLOCK_SIZE_BITS 10
#define BASE_BLOCK_SIZE     (1 << BASE_BLOCK_SIZE_BITS)
#define BASE_MAX_INODES     65535
#define UPPER(size,n) ((size+((n)-1))/(n))
#define BITS_PER_BLOCK (BASE_BLOCK_SIZE << 3)
#define MAX_GOOD_BLOCKS 512
#define BASE_INODES_PER_BLOCK ((BASE_BLOCK_SIZE)/(sizeof (struct base_inode)))

#define BASE_ROOT_INO 1


struct base_inode {
	uint16_t i_mode;
        uint16_t i_uid;
        uint32_t i_size;
        uint32_t i_time;
        uint8_t  i_gid;
	uint8_t  i_nlinks;
	uint16_t i_zone[9];
};

struct base_super_block {
	uint16_t s_ninodes;
        uint16_t s_nzones;
        uint16_t s_imap_blocks;
	uint16_t s_zmap_blocks;
	uint16_t s_firstdatazone;
	uint16_t s_log_zone_size;
        uint32_t s_max_size;
        uint16_t s_magic;
	uint16_t s_state;
        uint32_t s_zones;
};
  
static inline int xusleep(useconds_t usec)
{
#ifdef HAVE_NANOSLEEP
	struct timespec waittime = {
		.tv_sec   =  usec / 1000000L,
		.tv_nsec  = (usec % 1000000L) * 1000
	};
	return nanosleep(&waittime, NULL);
#elif defined(HAVE_USLEEP)
	return usleep(usec);
#endif	
//colocar um else	
}
static inline off_t first_zone_data(void)
{
	        return 2 + Super.s_imap_blocks + Super.s_zmap_blocks + UPPER(Super.s_ninodes, BASE_INODES_PER_BLOCK);
}

static inline off_t get_first_zone(void)
{

   return Super.s_firstdatazone;
}	
 static inline size_t get_inode_buffer_size(void)
{
	        return UPPER(Super.s_ninodes, BASE_INODES_PER_BLOCK)
			* BASE_BLOCK_SIZE;
}



#endif
