#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
   This file contains implementation of VESA library which is based on
   LRMI (Linux real-mode interface).
   So it's not an emulator - it calls real int 10h handler under Linux.
   Note: VESA is available only on x86 systems.
   You can redistribute this file under terms and conditions
   of GNU General Public licence v2.
   Written by Nickols_K <nickols_k@mail.ru>
*/
#include "vbelib.h"
#include "lrmi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include "mplayerxp.h"
#include "cpudetect.h"
#include "osdep_msg.h"

namespace	usr {

static struct VesaProtModeInterface vbe_pm_info;
static struct VesaModeInfoBlock curr_mode_info;

static inline int VERR(const any_t*p)
{
  register int retval;
  __asm __volatile(
	"xorl	%0, %0\n\t"
	"verr	%1\n\t"
	"setnz	%b0"
	:"=q"(retval)
	:"m"(*(unsigned char *)p)
	:"memory","cc");
  return retval;
}

#if 0
static inline int VERW(const any_t*p)
{
  register int retval;
  __asm __volatile(
	"xorl	%0, %0\n\t"
	"verw	%1\n\t"
	"setnz	%b0"
	:"=q"(retval)
	:"m"(*(unsigned char *)p)
	:"memory","cc");
  return retval;
}
#endif

#define HAVE_VERBOSE_VAR 1

#ifdef HAVE_VERBOSE_VAR

static void __dump_regs(struct LRMI_regs *r)
{
    mpxp_dbg2<<"vbelib:"
	<<"eax="<<std::hex<<(unsigned long)r->eax
	<<"ebx="<<std::hex<<(unsigned long)r->ebx
	<<"ecx="<<std::hex<<(unsigned long)r->ecx
	<<"edx="<<std::hex<<(unsigned long)r->edx<<std::endl;
    mpxp_dbg2<<"vbelib:"
	<<"edi="<<std::hex<<(unsigned long)r->edi
	<<"esi="<<std::hex<<(unsigned long)r->esi
	<<"ebp="<<std::hex<<(unsigned long)r->ebp
	<<"esp="<<std::hex<<(unsigned long)r->reserved<<std::endl;
    mpxp_dbg2<<"vbelib:"
	<<"ds="<<r->ds
	<<"es="<<r->es
	<<"ss="<<r->ss
	<<"fs="<<r->fs
	<<"gs="<<r->gs
	<<"cs="<<r->cs<<":ip="<<r->ip
	<<"ss="<<r->ss<<":sp="<<r->sp
	<<"flags="<<r->flags<<std::endl;
}

static inline int VBE_LRMI_int(int int_no, struct LRMI_regs *r)
{
  int retval;
  if(mp_conf.verbose > 1)
  {
    mpxp_dbg2<<"vbelib: registers before int "<<std::hex<<int_no<<std::endl;
    __dump_regs(r);
  }
  retval = LRMI_int(int_no,r);
  if(mp_conf.verbose > 1)
  {
    mpxp_dbg2<<"vbelib: Interrupt handler returns: "<<std::hex<<retval<<std::endl;
    mpxp_dbg2<<"vbelib: registers after int "<<std::hex<<int_no<<std::endl;
    __dump_regs(r);
  }
  return retval;
}
#else
#define VBE_LRMI_int(int_no,regs) (VBE_LRMI_int(int_no,regs))
#endif

static FILE *my_stdin;
static FILE *my_stdout;
static FILE *my_stderr;

static void __set_cursor_type(FILE *stdout_fd,int cursor_on)
{
  fprintf(stdout_fd,"\033[?25%c",cursor_on?'h':'l');
}

/* TODO: do it only on LCD or DFP. We should extract such info from DDC */
static void hide_terminal_output( void )
{
  my_stdin  = fopen(ttyname(fileno(stdin )),"r");
  my_stdout = fopen(ttyname(fileno(stdout)),"w");
  my_stderr = fopen(ttyname(fileno(stderr)),"w");
  __set_cursor_type(stdout,0);
/*if(isatty(fileno(stdin ))) stdin =freopen("/dev/null","r",stdin );*/
  if(isatty(fileno(stdout))) stdout=freopen("/dev/null","w",stdout);
  if(isatty(fileno(stderr))) stderr=freopen("/dev/null","w",stderr);
}

static void restore_terminal_output( void )
{
  stdin = my_stdin;
  stdout = my_stdout;
  stderr = my_stderr;
}

static unsigned hh_int_10_seg;
static int fd_mem;
/*
the list of supported video modes is stored in the reserved portion of
the SuperVGA information record by some implementations, and it may
thus be necessary to either copy the mode list or use a different
buffer for all subsequent VESA calls
*/
static any_t*controller_info;
int vbeInit( void )
{
   unsigned short iopl_port;
   size_t i;
   int retval;
   if(!LRMI_init()) return VBE_VM86_FAIL;
   if(!(controller_info = LRMI_alloc_real(sizeof(struct VbeInfoBlock)))) return VBE_OUT_OF_DOS_MEM;
   /*
    Allow read/write to ALL io ports
   */
   hh_int_10_seg = *(unsigned short *)PhysToVirtSO(0x0000,0x0042);
   /* Video BIOS should be at C000:0000 and above */
   hh_int_10_seg >>= 12;
   if(hh_int_10_seg < 0xC) return VBE_BROKEN_BIOS;
   ioperm(0, 1024, 1);
   iopl(3);
   memset(&vbe_pm_info,0,sizeof(struct VesaProtModeInterface));
   retval = vbeGetProtModeInfo(&vbe_pm_info);
   if(retval != VBE_OK) return retval;
   i = 0;
   if(vbe_pm_info.iopl_ports) /* Can be NULL !!!*/
   while((iopl_port=vbe_pm_info.iopl_ports[i]) != 0xFFFF
	 && vbe_pm_info.iopl_ports[i++] > 1023) ioperm(iopl_port,1,1);
   iopl(3);
   fd_mem = open("/dev/mem",O_RDWR);
   hide_terminal_output();
   return VBE_OK;
}

int vbeDestroy( void )
{
  __set_cursor_type(my_stdout,1);
  restore_terminal_output();
  close(fd_mem);
  LRMI_free_real(controller_info);
  return VBE_OK;
}

/* Fixme!!! This code is compatible only with mplayer's version of lrmi*/
static inline int is_addr_valid(const any_t*p)
{
  return (p < (const any_t*)0x502) ||
	 (p >= (const any_t*)0x10000 && p < (const any_t*)0x20000) ||
	 (p >= (const any_t*)0xa0000 && p < (const any_t*)0x100000);
}

static int check_str(const unsigned char *str)
{
  size_t i;
  int null_found = 0;
  for(i = 0;i < 256;i++)
  {
    if(is_addr_valid(&str[i]))
    {
      if(VERR(&str[i]))
      {
	if(!str[i]) { null_found = 1; break; }
      }
      else break;
    }
    else break;
  }
  return null_found;
}

static int check_wrd(const unsigned short *str)
{
  size_t i;
  int ffff_found = 0;
  for(i = 0;i < 1024;i++)
  {
    if(is_addr_valid(&str[i]))
    {
      if(VERR(&str[i]))
      {
	if(str[i] == 0xffff) { ffff_found = 1; break; }
      }
      else break;
    }
    else break;
  }
  return ffff_found;
}

static void print_str(const char *str)
{
  size_t i;
  fflush(stdout);
  mpxp_v<<"vbelib:    ";
  for(i = 0;i < 256;i++) { mpxp_v<<std::hex<<str[i]<<"("<<str[i]<<")"; if(!str[i]) break; }
  mpxp_v<<std::endl;
  mpxp_v.flush();
}

static void print_wrd(unsigned short *str)
{
  size_t i;
  mpxp_v<<"vbelib:    ";
  for(i = 0;i < 256;i++) { mpxp_v<<std::hex<<str[i]; if(str[i] == 0xffff) break; }
  mpxp_v<<std::endl;
  mpxp_v.flush();
}

int vbeGetControllerInfo(struct VbeInfoBlock *data)
{
  struct LRMI_regs r;
  int retval;
  memcpy(controller_info,data,sizeof(struct VbeInfoBlock));
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f00;
  r.es  = VirtToPhysSeg(controller_info);
  r.edi = VirtToPhysOff(controller_info);
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    FarPtr fpdata;
    retval = VBE_OK;
    memcpy(data,controller_info,sizeof(struct VbeInfoBlock));
    fpdata.seg = (unsigned long)(data->OemStringPtr) >> 16;
    fpdata.off = (unsigned long)(data->OemStringPtr) & 0xffff;
    data->OemStringPtr = (char*)PhysToVirt(fpdata);
    if(!check_str(reinterpret_cast<unsigned char*>(data->OemStringPtr))) data->OemStringPtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(mp_conf.verbose > 1)
    {
      mpxp_dbg2<<"vbelib:  OemStringPtr="<<std::hex<<fpdata.seg<<":"<<std::hex<<fpdata.off<<" => "<<data->OemStringPtr<<std::endl;
      if(data->OemStringPtr) print_str(data->OemStringPtr);
      fflush(stdout);
    }
#endif
    fpdata.seg = (unsigned long)(data->VideoModePtr) >> 16;
    fpdata.off = (unsigned long)(data->VideoModePtr) & 0xffff;
    data->VideoModePtr = (unsigned short*)PhysToVirt(fpdata);
    if(!check_wrd(data->VideoModePtr))
    {
	data->VideoModePtr = NULL;
	retval = VBE_BROKEN_BIOS;
    }
#ifdef HAVE_VERBOSE_VAR
    if(mp_conf.verbose > 1)
    {
      mpxp_dbg2<<"vbelib:  VideoModePtr="<<std::hex<<fpdata.seg<<":"<<std::hex<<fpdata.off<<" => "<<data->VideoModePtr<<std::endl;
      if(data->VideoModePtr) print_wrd(data->VideoModePtr);
      fflush(stdout);
    }
#endif
    fpdata.seg = (unsigned long)(data->OemVendorNamePtr) >> 16;
    fpdata.off = (unsigned long)(data->OemVendorNamePtr) & 0xffff;
    data->OemVendorNamePtr = (char *)PhysToVirt(fpdata);
    if(!check_str(reinterpret_cast<unsigned char*>(data->OemVendorNamePtr))) data->OemVendorNamePtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(mp_conf.verbose > 1)
    {
      mpxp_dbg2<<"vbelib:  OemVendorNamePtr="<<std::hex<<fpdata.seg<<":"<<std::hex<<fpdata.off<<" => "<<data->OemVendorNamePtr<<std::endl;
      if(data->OemVendorNamePtr) print_str(data->OemVendorNamePtr);
      fflush(stdout);
    }
#endif
    fpdata.seg = (unsigned long)(data->OemProductNamePtr) >> 16;
    fpdata.off = (unsigned long)(data->OemProductNamePtr) & 0xffff;
    data->OemProductNamePtr = (char*)PhysToVirt(fpdata);
    if(!check_str(reinterpret_cast<unsigned char*>(data->OemProductNamePtr))) data->OemProductNamePtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(mp_conf.verbose > 1)
    {
      mpxp_dbg2<<"vbelib:  OemProductNamePtr="<<std::hex<<fpdata.seg<<":"<<std::hex<<fpdata.off<<" => "<<data->OemProductNamePtr<<std::endl;
      if(data->OemVendorNamePtr) print_str(data->OemProductNamePtr);
      fflush(stdout);
    }
#endif
    fpdata.seg = (unsigned long)(data->OemProductRevPtr) >> 16;
    fpdata.off = (unsigned long)(data->OemProductRevPtr) & 0xffff;
    data->OemProductRevPtr = (char*)PhysToVirt(fpdata);
    if(!check_str(reinterpret_cast<unsigned char*>(data->OemProductRevPtr))) data->OemProductRevPtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(mp_conf.verbose > 1)
    {
      mpxp_dbg2<<"vbelib:  OemProductRevPtr="<<std::hex<<fpdata.seg<<":"<<std::hex<<fpdata.off<<" => "<<data->OemProductRevPtr<<std::endl;
      if(data->OemProductRevPtr) print_str(data->OemProductRevPtr);
      fflush(stdout);
    }
#endif
  }
  return retval;
}

int vbeGetModeInfo(unsigned mode,struct VesaModeInfoBlock *data)
{
  struct LRMI_regs r;
  any_t*rm_space;
  int retval;
  if(!(rm_space = LRMI_alloc_real(sizeof(struct VesaModeInfoBlock)))) return VBE_OUT_OF_DOS_MEM;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f01;
  r.ecx = mode;
  r.es  = VirtToPhysSeg(rm_space);
  r.edi = VirtToPhysOff(rm_space);
  if(!VBE_LRMI_int(0x10,&r))
  {
     LRMI_free_real(rm_space);
     return VBE_VM86_FAIL;
  }
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    retval = VBE_OK;
    memcpy(data,rm_space,sizeof(struct VesaModeInfoBlock));
  }
  LRMI_free_real(rm_space);
  return retval;
}

int vbeSetMode(unsigned mode,struct VesaCRTCInfoBlock *data)
{
  struct LRMI_regs r;
  any_t*rm_space = NULL;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  if(data)
  {
    if(!(rm_space = LRMI_alloc_real(sizeof(struct VesaCRTCInfoBlock)))) return VBE_OUT_OF_DOS_MEM;
    r.es  = VirtToPhysSeg(rm_space);
    r.edi = VirtToPhysOff(rm_space);
    memcpy(rm_space,data,sizeof(struct VesaCRTCInfoBlock));
  }
  r.eax = 0x4f02;
  r.ebx = mode;
  retval = VBE_LRMI_int(0x10,&r);
  LRMI_free_real(rm_space);
  if(!retval) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    /* Just info for internal use (currently in SetDiplayStart func). */
    vbeGetModeInfo(mode&0x1f,&curr_mode_info);
    retval = VBE_OK;
  }
  return retval;
}

int vbeGetMode(unsigned *mode)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f03;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    *mode = r.ebx;
    retval = VBE_OK;
  }
  return retval;
}

int vbeSaveState(any_t**data)
{
  struct LRMI_regs r;
  int retval;
  any_t*rm_space;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f04;
  r.edx = 0x00;
  r.ecx = 0x0f;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval != 0x4f) return retval;
  if(!(rm_space = LRMI_alloc_real((r.ebx & 0xffff)*64))) return VBE_OUT_OF_DOS_MEM;
  r.eax = 0x4f04;
  r.edx = 0x01;
  r.ecx = 0x0f;
  r.es  = VirtToPhysSeg(rm_space);
  r.ebx = VirtToPhysOff(rm_space);
  if(!VBE_LRMI_int(0x10,&r))
  {
    LRMI_free_real(rm_space);
    return VBE_VM86_FAIL;
  }
  retval = r.eax & 0xffff;
  if(retval != 0x4f)
  {
    LRMI_free_real(rm_space);
    return retval;
  }
  *data = rm_space;
  return VBE_OK;
}

int vbeRestoreState(any_t*data)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f04;
  r.edx = 0x02;
  r.ecx = 0x0f;
  r.es  = VirtToPhysSeg(data);
  r.ebx = VirtToPhysOff(data);
  retval = VBE_LRMI_int(0x10,&r);
  LRMI_free_real(data);
  if(!retval) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

int vbeGetWindow(unsigned *win_num)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f05;
  r.ebx = (*win_num & 0x0f) | 0x0100;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    *win_num = r.edx & 0xffff;
    retval = VBE_OK;
  }
  return retval;
}

int vbeSetWindow(unsigned win_num,unsigned win_gran)
{
  int retval;
  if(vbe_pm_info.SetWindowCall)
  {
     /* Don't mp_conf.verbose this stuff from performance reasons */
     /* 32-bit function call is much better of int 10h */
     __asm __volatile(
	"pushl	%%"REG_b"\n"
	"movl	%1, %%"REG_b"\n"
	::"a"(0x4f05),"S"(win_num & 0x0f),"d"(win_gran):"memory");
    (*vbe_pm_info.SetWindowCall)();
    __asm __volatile("popl	%%"REG_b"":::"memory");
    retval = VBE_OK;
  }
  else
  {
    struct LRMI_regs r;
    memset(&r,0,sizeof(struct LRMI_regs));
    r.eax = 0x4f05;
    r.ebx = win_num & 0x0f;
    r.edx = win_gran;
    if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
    retval = r.eax & 0xffff;
    if(retval == 0x4f) retval = VBE_OK;
  }
  return retval;
}

int vbeGetScanLineLength(unsigned *num_pixels,unsigned *num_bytes)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f06;
  r.ebx = 1;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    if(num_bytes)  *num_bytes = r.ebx & 0xffff;
    if(num_pixels) *num_pixels= r.ecx & 0xffff;
    retval = VBE_OK;
  }
  return retval;
}

int vbeGetMaxScanLines(unsigned *num_pixels,unsigned *num_bytes, unsigned *num_lines)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f06;
  r.ebx = 3;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    if(num_bytes)  *num_bytes = r.ebx & 0xffff;
    if(num_pixels) *num_pixels= r.ecx & 0xffff;
    if(num_lines)  *num_lines = r.edx & 0xffff;
    retval = VBE_OK;
  }
  return retval;
}

int vbeSetScanLineLength(unsigned num_pixels)
{
  int retval;
  struct LRMI_regs r;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f06;
  r.ebx = 0;
  r.ecx = num_pixels;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

int vbeSetScanLineLengthB(unsigned num_bytes)
{
  int retval;
  struct LRMI_regs r;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f06;
  r.ebx = 2;
  r.ecx = num_bytes;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

int vbeGetDisplayStart(unsigned *pixel_num,unsigned *scan_line)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f07;
  r.ebx = 1;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    if(pixel_num) *pixel_num = r.ecx & 0xffff;
    if(scan_line) *scan_line = r.edx & 0xffff;
    retval = VBE_OK;
  }
  return retval;
}

int vbeSetDisplayStart(unsigned long offset, int vsync)
{
  int retval;
  if(vbe_pm_info.SetDisplayStart)
  {
     /* Don't mp_conf.verbose this stuff from performance reasons */
     /* 32-bit function call is much better of int 10h */
     __asm __volatile(
	"pushl	%%"REG_b"\n"
	"movl	%1, %%"REG_b"\n"
	::"a"(0x4f07),"S"(vsync ? 0x80 : 0),
	  "c"((offset>>2) & 0xffff),"d"((offset>>18)&0xffff):"memory");
    (*vbe_pm_info.SetDisplayStart)();
    __asm __volatile("popl	%%"REG_b:::"memory");
    retval = VBE_OK;
  }
  else
  {
#if 0
    /* Something wrong here */
    struct LRMI_regs r;
    unsigned long pixel_num;
    memset(&r,0,sizeof(struct LRMI_regs));
    pixel_num = offset%(unsigned long)curr_mode_info.BytesPerScanLine;
    if(pixel_num*(unsigned long)curr_mode_info.BytesPerScanLine!=offset) pixel_num++;
    r.eax = 0x4f07;
    r.ebx = vsync ? 0x82 : 2;
    r.ecx = pixel_num;
    r.edx = offset/(unsigned long)curr_mode_info.BytesPerScanLine;
    if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
    retval = r.eax & 0xffff;
    if(retval == 0x4f) retval = VBE_OK;
#endif
    retval = VBE_BROKEN_BIOS;
  }
  return retval;
}

int vbeSetScheduledDisplayStart(unsigned long offset, int vsync)
{
  int retval;
  struct LRMI_regs r;
  unsigned long pixel_num;
  memset(&r,0,sizeof(struct LRMI_regs));
  pixel_num = offset%(unsigned long)curr_mode_info.BytesPerScanLine;
  if(pixel_num*(unsigned long)curr_mode_info.BytesPerScanLine!=offset) pixel_num++;
  r.eax = 0x4f07;
  r.ebx = vsync ? 0x82 : 2;
  r.ecx = offset;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

struct realVesaProtModeInterface
{
  unsigned short SetWindowCall;
  unsigned short SetDisplayStart;
  unsigned short SetPaletteData;
  unsigned short iopl_ports;
}__attribute__((packed));

int vbeGetProtModeInfo(struct VesaProtModeInterface *pm_info)
{
  struct LRMI_regs r;
  int retval;
  unsigned info_offset;
  struct realVesaProtModeInterface *rm_info;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f0a;
  r.ebx = 0;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    retval = VBE_OK;
    info_offset = r.edi&0xffff;
    if((r.es >> 12) != hh_int_10_seg) retval = VBE_BROKEN_BIOS;
    rm_info = (realVesaProtModeInterface*)PhysToVirtSO(r.es,info_offset);
    pm_info->SetWindowCall   = (void (*)())PhysToVirtSO(r.es,info_offset+rm_info->SetWindowCall);
    if(!is_addr_valid(reinterpret_cast<any_t*>(pm_info->SetWindowCall))) retval = VBE_BROKEN_BIOS;
#ifdef HAVE_VERBOSE_VAR
    mpxp_dbg2<<"vbelib:  SetWindowCall="<<std::hex<<r.es<<":"<<std::hex<<(info_offset+rm_info->SetWindowCall)<<" => "<<pm_info->SetWindowCall<<std::endl;
#endif
    pm_info->SetDisplayStart = (void(*)())PhysToVirtSO(r.es,info_offset+rm_info->SetDisplayStart);
    if(!is_addr_valid(reinterpret_cast<any_t*>(pm_info->SetDisplayStart))) retval = VBE_BROKEN_BIOS;
#ifdef HAVE_VERBOSE_VAR
    mpxp_dbg2<<"vbelib:  SetDisplayStart="<<std::hex<<r.es<<":"<<std::hex<<(info_offset+rm_info->SetDisplayStart)<<" => "<<pm_info->SetDisplayStart<<std::endl;
#endif
    pm_info->SetPaletteData  = (void(*)())PhysToVirtSO(r.es,info_offset+rm_info->SetPaletteData);
    if(!is_addr_valid(reinterpret_cast<any_t*>(pm_info->SetPaletteData))) retval = VBE_BROKEN_BIOS;
#ifdef HAVE_VERBOSE_VAR
    mpxp_dbg2<<"vbelib:  SetPaletteData="<<std::hex<<r.es<<":"<<std::hex<<(info_offset+rm_info->SetPaletteData)<<" => "<<pm_info->SetPaletteData<<std::endl;
#endif
    pm_info->iopl_ports  = (unsigned short*)PhysToVirtSO(r.es,info_offset+rm_info->iopl_ports);
    if(!rm_info->iopl_ports) pm_info->iopl_ports = NULL;
    else
    if(!check_wrd(pm_info->iopl_ports))
    {
	pm_info->iopl_ports = NULL;
/*	retval = VBE_BROKEN_BIOS; <- It's for broken BIOSes only */
    }
#ifdef HAVE_VERBOSE_VAR
    if(mp_conf.verbose > 1)
    {
      mpxp_dbg2<<"vbelib:  iopl_ports="<<std::hex<<r.es<<":"<<std::hex<<(info_offset+rm_info->iopl_ports)<<" => "<<pm_info->iopl_ports<<std::endl;
      if(pm_info->iopl_ports) print_wrd(pm_info->iopl_ports);
      fflush(stdout);
    }
#endif
  }
  return retval;
}
/* --------- Standard VGA stuff -------------- */
int vbeWriteString(int x, int y, int attr,const char *str)
{
  struct LRMI_regs r;
  any_t*rm_space = NULL;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.ecx = strlen(str);
  r.edx = ((y<<8)&0xff00)|(x&0xff);
  r.ebx = attr;
  if(!(rm_space = LRMI_alloc_real(r.ecx))) return VBE_OUT_OF_DOS_MEM;
  r.es  = VirtToPhysSeg(rm_space);
  r.ebp = VirtToPhysOff(rm_space);
  memcpy(rm_space,str,r.ecx);
  r.eax = 0x1300;
  retval = VBE_LRMI_int(0x10,&r);
  LRMI_free_real(rm_space);
  if(!retval) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

any_t* vbeMapVideoBuffer(unsigned long phys_addr,unsigned long size)
{
  any_t*lfb;
  if(fd_mem == -1) return NULL;
  mpxp_dbg2<<"vbelib: vbeMapVideoBuffer("<<std::hex<<phys_addr<<","<<std::hex<<size<<")"<<std::endl;
  /* Here we don't need with MAP_FIXED and prefered address (first argument) */
  lfb = mmap((any_t*)0,size,PROT_READ | PROT_WRITE,MAP_SHARED,fd_mem,phys_addr);
  return lfb == (any_t*)-1 ? 0 : lfb;
}

void vbeUnmapVideoBuffer(unsigned long linear_addr,unsigned long size)
{
  mpxp_dbg2<<"vbelib: vbeUnmapVideoBuffer("<<std::hex<<linear_addr<<","<<std::hex<<size<<")"<<std::endl;
  munmap((any_t*)linear_addr,size);
}

} // namespace	usr
