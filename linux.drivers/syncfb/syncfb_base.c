/*
 *
 * syncfb.c
 *
 * written by Matthias Oelmann
 * 
 * based on mga_vid.c by Aaron Holzmann
 * Module skeleton based on gutted agpgart module by Jeff Hartmann 
 * <slicer@ionet.net>
 *
 * Synchronous Framebuffer Module Version 0.0.1
 * 
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */

//It's entirely possible this major conflicts with something else ( mga_vid? ;-) )
/* mknod /dev/syncfb c 178 0 */

#include <linux/version.h>
#include <linux/module.h>
#include "syncfb.h"

#undef VERSION
#define VERSION(major,minor,patch) (((((major)<<8)+(minor))<<8)+(patch))

MODULE_AUTHOR("Matthias Oelmann <mao@well.com>");


/*
	PROTOTYPES
*/
static void syncfb_request_buffer(syncfb_buffer_info_t *sfb_info);
static void syncfb_commit_buffer(syncfb_buffer_info_t *sfb_info);
static syncfb_device_t *syncfb_probe_devices(void);

/* basics */
static int syncfb_in_use = 0;
static syncfb_device_t *hw = NULL;		/* the hardware dependent part */

/* #if LINUX_VERSION_CODE < VERSION(2,3,0)
typedef struct wait_queue *wait_queue_head_t;
#define DECLARE_WAITQUEUE(name,task) struct wait_queue (name) = {(task),NULL}
#define init_waitqueue_head(head) *(head) = NULL
#define set_current_state(a) current->state = (a)
#endif */

/* block on request_buffer support */
static int waiting = 0;
static wait_queue_head_t waiter;

static syncfb_config_t sfb_config;		/* current configuration data */
static syncfb_status_info_t sfb_status_info; 	/* status info data */


/*
	FIFO VARS
*/

#define FIFO_MAX_SIZE 4

static int sfb_fifo_size = FIFO_MAX_SIZE;

static int sfb_fifo_status[FIFO_MAX_SIZE]; 	/* LIVE or WAIT or OFFS or FREE */
static int sfb_fifo_queue[FIFO_MAX_SIZE];  	/* the actual fifo */
static int sfb_fifo_repeat[FIFO_MAX_SIZE];
static int sfb_default_repeat = 1;

static int sfb_fifo_nptr = 0;  			/* next entry pointer */
static int sfb_fifo_sptr = 0;  			/* next show pointer */

static int sfb_fifo_cframe; 			/* current frame buffer */

static int sfb_fifo_wait_cnt;   		/* number of buffers waiting to be displayed */
static int sfb_fifo_in_use_cnt = 0; 		/* total cnt: LIVE + WAIT + OFFS */

static int sfb_fifo_skip_field_flag = 0; 	/* tells the interrupt handler to skip one frame */
static int sfb_mode_blocking = 0;
/*
	STATISTICS
*/
static uint_32 sfb_stat_field_cnt;
static uint_32 sfb_stat_skip_field_cnt;
static uint_32 sfb_stat_hold_field_cnt;

static uint_32 sfb_stat_request_frames;
static uint_32 sfb_stat_failed_requests;

static uint_32 sfb_stat_commit_frames;
static uint_32 sfb_stat_frame_cnt;



static uint_8 sfb_bits_per_pixel[32] = {
        8,      /* UNKNOWN                 0     */
        8,      /* VIDEO_PALETTE_GREY      1        Linear greyscale */
        8,      /* VIDEO_PALETTE_HI240     2        High 240 cube (BT848) */
        16,     /* VIDEO_PALETTE_RGB565    3        565 16 bit RGB */
        24,     /* VIDEO_PALETTE_RGB24     4        24bit RGB */
        32,     /* VIDEO_PALETTE_RGB32     5        32bit RGB */
        16,     /* VIDEO_PALETTE_RGB555    6        555 15bit RGB */
        16,     /* VIDEO_PALETTE_YUV422    7        YUV422 capture */
        16,     /* VIDEO_PALETTE_YUYV      8   */
        16,     /* VIDEO_PALETTE_UYVY      9        The great thing about standards is ... */
        12,     /* VIDEO_PALETTE_YUV420    10  */
        12,     /* VIDEO_PALETTE_YUV411    11       YUV411 capture */
        32,     /* !!! VIDEO_PALETTE_RAW   12       RAW capture (BT848) */
        16,     /* VIDEO_PALETTE_YUV422P   13       YUV 4:2:2 Planar */
        12,     /* VIDEO_PALETTE_YUV411P   14       YUV 4:1:1 Planar */
        12,     /* VIDEO_PALETTE_YUV420P   15       YUV 4:2:0 Planar */
        10,     /* VIDEO_PALETTE_YUV410P   16       YUV 4:1:0 Planar */
        16,     /* VIDEO_PALETTE_YUV422P2  17       YUV 4:2:2 2-Plane */
        12,     /* VIDEO_PALETTE_YUV411P2  18       YUV 4:1:1 2-Plane */
        12,     /* VIDEO_PALETTE_YUV420P2  19       YUV 4:2:0 2-Plane */
        10,     /* VIDEO_PALETTE_YUV410P2  20       YUV 4:1:0 2-Plane */
        32,
        32,
        32,
        32,
        32,
        32,
        32,
        32,
        32,
        32,
        32
};


uint_8 syncfb_get_bpp(uint_8 pal) {
	if ( pal < 32 )
		return sfb_bits_per_pixel[pal];
	else
		return 0;
}


/* return 0 on success, errcode (-1) on failure */
static int syncfb_initialize() {

	hw = syncfb_probe_devices();
	if ( hw == NULL ) return -1;
	init_waitqueue_head(&waiter);

	printk(KERN_DEBUG "Found Syncfb Device: %s\n", hw->capability()->name);	
	return 0;
}

static void syncfb_turn_on() {
	int i;
	int cf = sfb_fifo_cframe;

	if ( cf >= sfb_fifo_size ) cf = 0;

	for ( i=0; i < sfb_fifo_size; i++ ) {
		sfb_fifo_status[i] = SFB_STATUS_FREE;
	}
	
	sfb_fifo_nptr = 0;
	sfb_fifo_sptr = 0;
	sfb_fifo_status[cf] = SFB_STATUS_LIVE;
	sfb_fifo_repeat[cf] = 1;
	sfb_fifo_cframe = cf;
	sfb_fifo_in_use_cnt = 1;
	sfb_fifo_wait_cnt = 0;
	sfb_fifo_skip_field_flag = 0;
	hw->enable();
	hw->select_buffer(sfb_fifo_cframe);
}

static int syncfb_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

	syncfb_buffer_info_t sfb_info;	

	switch(cmd) {

		 case SYNCFB_GET_CAPS:
                        if( copy_to_user((void*) arg, hw->capability(), sizeof(syncfb_capability_t)))
                        {
                                printk(KERN_ERR "syncfb: SYNCFB_GET_CAPS failed copy to userspace\n");
                                return(-EFAULT);
                        }
			break;

		 case SYNCFB_GET_CONFIG:
			hw->configure(0, &sfb_config);
                        if( copy_to_user((void*) arg, &sfb_config, sizeof(syncfb_config_t)))
                        {
                                printk(KERN_ERR "syncfb: SYNCFB_GET_CONFIG failed copy to userspace\n");
                                return(-EFAULT);
                        }
			break;
			
		 case SYNCFB_SET_CONFIG:
                        printk(KERN_DEBUG "syncfb: Received configuration\n");
                        if(copy_from_user(&sfb_config,(syncfb_config_t*) arg,sizeof(syncfb_config_t)))
                        {
                                printk(KERN_ERR "syncfb: SYNCFB_SET_CONFIG failed copy from userspace\n");
                                return(-EFAULT);
                        }
			sfb_default_repeat = sfb_config.default_repeat;
			sfb_mode_blocking =  sfb_config.syncfb_mode & SYNCFB_FEATURE_BLOCK_REQUEST;
			hw->configure(1, &sfb_config);
			sfb_fifo_size = sfb_config.buffers;
			syncfb_turn_on();
                        if( copy_to_user((void*) arg, &sfb_config, sizeof(syncfb_config_t)))
                        {
                                printk(KERN_ERR "syncfb: SYNCFB_GET_CONFIG failed copy to userspace\n");
                                return(-EFAULT);
                        }
			break;


		case SYNCFB_REQUEST_BUFFER:
                        if(copy_from_user(&sfb_info,(syncfb_buffer_info_t*) arg,sizeof(syncfb_buffer_info_t)))
			{
                                printk(KERN_ERR "syncfb: SYNCFB_REQUEST_BUFFER failed copy from userspace\n");
                                return(-EFAULT);
			}
			syncfb_request_buffer(&sfb_info);
                        if( copy_to_user((void*) arg, &sfb_info, sizeof(syncfb_buffer_info_t)))
                        {
                                printk(KERN_ERR "syncfb: SYNCFB_REQUEST_BUFFER failed copy to userspace\n");
                                return(-EFAULT);
                        }
			break;

		case SYNCFB_STATUS:
			sfb_status_info.field_cnt = sfb_stat_field_cnt;
			sfb_status_info.skip_field_cnt = sfb_stat_skip_field_cnt;
			sfb_status_info.hold_field_cnt = sfb_stat_hold_field_cnt;
                        if( copy_to_user((void*) arg, &sfb_status_info, sizeof(syncfb_status_info_t)))
                        {
                                printk(KERN_ERR "syncfb: SYNCFB_STATUS failed copy to userspace\n");
                                return(-EFAULT);
                        }
			break;

		case SYNCFB_COMMIT_BUFFER:
                        if(copy_from_user(&sfb_info,(syncfb_buffer_info_t*) arg,sizeof(syncfb_buffer_info_t)))
			{
                                printk(KERN_ERR "syncfb: SYNCFB_COMMIT_BUFFER failed copy from userspace\n");
                                return(-EFAULT);
			}
			syncfb_commit_buffer(&sfb_info);
			break;

		case SYNCFB_ON:
			syncfb_turn_on();
			break;

		case SYNCFB_OFF:
			hw->disable();
			break;

		default:
			if ( hw->ioctl )
				return hw->ioctl(inode, file, cmd, arg);
			else
				return (-EINVAL);

	}

	return 0;
}



static void syncfb_request_buffer(syncfb_buffer_info_t *sfb_info) {
	int i;

	/* some statistics */
	sfb_stat_request_frames++;

	if ( sfb_mode_blocking && (sfb_fifo_in_use_cnt == sfb_fifo_size) ) {
		waiting = 1;
		interruptible_sleep_on(&waiter);
		waiting = 0;
	}

	for (i=0; i<sfb_fifo_size; i++) {
		if (sfb_fifo_status[i] == SFB_STATUS_FREE) {
			sfb_fifo_status[i] = SFB_STATUS_OFFS;
			if( ++sfb_fifo_in_use_cnt == sfb_fifo_size && sfb_mode_blocking == 0 ) sfb_fifo_skip_field_flag = 1; 
			sfb_info->id = i;
			sfb_info->repeat = sfb_default_repeat;  /* TBD set default repeat rate, fill in offset & size */
			hw->request_buffer(sfb_info);
			// printk ("syncfb: request_buffer success #%d\n", i);
			return;
		} else {
		//	printk ("syncfb: request_buffer fails for #%d with status %d\n", i, sfb_fifo_status[i]);

		}
	}


	sfb_stat_failed_requests++;
	sfb_info->id = -1;

	return; /* no available buffer */
}

static void syncfb_commit_buffer(syncfb_buffer_info_t *sfb_info) {

	int bid = sfb_info->id;

	/* some statistics */
	sfb_stat_commit_frames++;

	if ( bid < 0 || bid >= sfb_fifo_size ) {
		/* printk("syncfb: commited buffer has invalid id: %d\n", bid); */
		return;
	}

	if ( sfb_fifo_status[bid] != SFB_STATUS_OFFS ) {
		printk(KERN_ERR "syncfb: commited buffer has wrong status (!= OFFS)\n");
		return; /*error*/
	}

	sfb_fifo_status[bid] = SFB_STATUS_WAIT;
	sfb_fifo_repeat[bid] = sfb_info->repeat;
	
	sfb_fifo_queue[sfb_fifo_nptr++] = bid;
	if ( sfb_fifo_nptr == sfb_fifo_size ) sfb_fifo_nptr = 0;

	sfb_fifo_wait_cnt++;
	return;
}




/*
 *	Interrupt handler: called by syncfb_device on every VerticalBlank
 */
int syncfb_interrupt() {

	sfb_stat_field_cnt++;
	
	/* if no buffers are waiting to be displayed, just hold the current frame */
	if ( sfb_fifo_wait_cnt == 0 ) { 
		sfb_stat_hold_field_cnt++; 
		return 0; /* hold, no new field */
	}  /* nothing to do */


	/* check for repeat count - if not null do nothing */
	if ( --sfb_fifo_repeat[sfb_fifo_cframe] > 0 ) {
		if (sfb_fifo_skip_field_flag) {
			sfb_fifo_skip_field_flag = 0;
			sfb_stat_skip_field_cnt++;
			if ( --sfb_fifo_repeat[sfb_fifo_cframe] > 0 ) {
				//printk("syncfb: interrupt repeat 2\n");
				return 0;
			}
		} else {
				//printk("syncfb: interrupt repeat 1\n");
			return 0;
		}
	}

	sfb_fifo_status[sfb_fifo_cframe] = SFB_STATUS_FREE;
	// printk("syncfb: freeing buffer #%d\n", sfb_fifo_cframe);
	sfb_fifo_in_use_cnt--;

	sfb_fifo_cframe = sfb_fifo_queue[sfb_fifo_sptr++];
	if ( sfb_fifo_sptr == sfb_fifo_size ) sfb_fifo_sptr = 0;

	sfb_fifo_status[sfb_fifo_cframe] = SFB_STATUS_LIVE;
	// printk("syncfb: buffer #%d is live now\n", sfb_fifo_cframe);
	hw->select_buffer(sfb_fifo_cframe);

	if ( waiting ) wake_up_interruptible(&waiter);

	sfb_stat_frame_cnt++;
	sfb_fifo_wait_cnt--;
	return 1;
}

static syncfb_device_t *syncfb_probe_devices(void) {

	syncfb_device_t *dev;

#ifdef SYNCFB_MATROX_SUPPORT	
	dev = syncfb_get_matrox_device();	
	if ( dev != NULL ) return dev;
#endif

#ifdef SYNCFB_GENERIC_SUPPORT	
	dev = syncfb_get_generic_device();	
	if ( dev != NULL ) return dev;
#endif

	return NULL;
}


/* ************************************************************

		FILE OPS

   ************************************************************ */



loff_t syncfb_lseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}
					 
static ssize_t syncfb_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t syncfb_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int syncfb_mmap(struct file *file, struct vm_area_struct *vma)
{
	if ( hw->mmap ) {
		return hw->mmap(file,vma);
	} else {
		return -EINVAL;
	}
}

static int syncfb_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);

	if(minor != 0)
		return(-ENXIO);

	if(syncfb_in_use == 1) 
		return(-EBUSY);

	syncfb_in_use = 1;
	MOD_INC_USE_COUNT; /* FIXME turn me back on! done. */
	return(0);
}

static int syncfb_release(struct inode *inode, struct file *file)
{
	syncfb_in_use = 0;
        MOD_DEC_USE_COUNT;  /* FIXME put back in! done. */
	return 0;
}

#if LINUX_VERSION_CODE >= 0x020400
static struct file_operations syncfb_fops =
{
	llseek:		syncfb_lseek,
	read:		syncfb_read,
	write:		syncfb_write,
	ioctl:		syncfb_ioctl,
	mmap:		syncfb_mmap,
	open:		syncfb_open,
	release:	syncfb_release
};
#else
static struct file_operations syncfb_fops =
{
	syncfb_lseek,
	syncfb_read,
	syncfb_write,
	NULL,
	NULL,
	syncfb_ioctl,
	syncfb_mmap,
	syncfb_open,
	NULL,
	syncfb_release,
};
#endif






/* ************************************************************

		KERNEL MODULE

   ************************************************************ */


int init_module(void)
{
	syncfb_in_use = 0;

	printk(KERN_DEBUG "Synchronous Framebuffer Module (syncfb)\n");
	if(register_chrdev(SYNCFB_MAJOR, "syncfb", &syncfb_fops))
	{
		printk(KERN_ERR "syncfb: unable to get major: %d\n", SYNCFB_MAJOR);
		return -EIO;
	}
	return syncfb_initialize();
}

void cleanup_module(void)
{

	printk(KERN_DEBUG "syncfb: Cleaning up module\n");
	if (hw) hw->cleanup();

	printk(KERN_DEBUG "syncfb: Unregister chrdev\n");
	unregister_chrdev(SYNCFB_MAJOR, "syncfb");

	printk(KERN_DEBUG "syncfb: Module removed\n");
}



