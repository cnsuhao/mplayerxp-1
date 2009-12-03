/*
 *
 * mga_vid_test.c
 *
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * Sept 1999
 *
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev.h>
#include "syncfb.h"

syncfb_capability_t caps;


int 
main(int argc, char *argv[])
{
	int f;


	f = open("/dev/syncfb",O_RDWR);

	if(f == -1)
	{
		f = open("/dev/mga_vid",O_RDWR);
		if ( f == -1 ) {
			fprintf(stderr,"Couldn't open driver\n");
			exit(1);
		}
		printf("Opening /dev/mga_vid instead of /dev/syncfb\n");
	}


	if (ioctl(f,SYNCFB_GET_CAPS,&caps)) perror("Error in config ioctl");
	printf("Syncfb device name is '%s'\n", caps.name);


	if (ioctl(f,SYNCFB_OFF,0)) perror("Error in ON ioctl");

	close(f);
	return 0;
}
