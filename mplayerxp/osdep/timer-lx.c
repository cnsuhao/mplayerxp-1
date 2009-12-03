// Precise timer routines for LINUX  (C) LGB & A'rpi/ASTRAL

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "../config.h"
#ifdef HAVE_RTC
#include <linux/rtc.h>
#endif
#include <fcntl.h>
#include "osdep_msg.h"


int usec_sleep(int usec_delay)
{
#ifdef HAVE_NANOSLEEP
    struct timespec ts;
    ts.tv_sec  =  usec_delay / 1000000;
    ts.tv_nsec = (usec_delay % 1000000) * 1000;
    return nanosleep(&ts, NULL);
#else
    return usleep(usec_delay);
#endif
}


// Returns current time in microseconds
unsigned int GetTimer(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}  

// Returns current time in milliseconds
unsigned int GetTimerMS(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000+tv.tv_usec/1000);
}  

static unsigned int RelativeTime=0;

// Returns time spent between now and last call in seconds
float GetRelativeTime(){
unsigned int t,r;
  t=GetTimer();
  r=t-RelativeTime;
  RelativeTime=t;
  return (float)r * 0.000001F;
}

// Initialize timer, must be called at least once at start
int InitTimer(void){
  int rtc_fd=-1;
  GetRelativeTime();
#ifdef HAVE_RTC
  if ((rtc_fd = open("/dev/rtc", O_RDONLY)) < 0) perror ("Linux RTC init: open");
  else {
	unsigned long irqp;

	/* if (ioctl(rtc_fd, RTC_IRQP_SET, _) < 0) */
	if (ioctl(rtc_fd, RTC_IRQP_READ, &irqp) < 0) {
    		perror ("Linux RTC init: ioctl (rtc_irqp_read)");
    		close (rtc_fd);
    		rtc_fd = -1;
	} else if (ioctl(rtc_fd, RTC_PIE_ON, 0) < 0) {
		/* variable only by the root */
    		perror ("Linux RTC init: ioctl (rtc_pie_on)");
    		close (rtc_fd);
		rtc_fd = -1;
	} else
		MSG_V("Using Linux's hardware RTC timing (%ldHz)\n", irqp);
  }
#endif
  return rtc_fd;
}

float SleepTime(int rtc_fd,int softsleep,float time_frame)
{
#ifdef HAVE_RTC
    if(rtc_fd>=0){
	// -------- RTC -----------
        while (time_frame > 0.000) {
	    unsigned long rtc_ts;
	    if (read (rtc_fd, &rtc_ts, sizeof(rtc_ts)) <= 0)
		    MSG_ERR( "Linux RTC read error: %s\n", strerror(errno));
    	    time_frame-=GetRelativeTime();
	}
    } else
#endif
    {
	// -------- USLEEP + SOFTSLEEP -----------
	float min=softsleep?0.021:0.005;
        while(time_frame>min){
          if(time_frame<=0.020)
             usec_sleep(0); // sleeps 1 clock tick (10ms)!
          else
             usec_sleep(1000000*(time_frame-0.020));
          time_frame-=GetRelativeTime();
        }
	if(softsleep){
	    if(time_frame<0) MSG_WARN( "Warning! Softsleep underflow!\n");
	    while(time_frame>0) time_frame-=GetRelativeTime(); // burn the CPU
	}
    }
    return time_frame;
}

#if 0
void main(){
  float t=0;
  InitTimer();
  while(1){ t+=GetRelativeTime();printf("time= %10.6f\r",t);fflush(stdout); }
}
#endif

