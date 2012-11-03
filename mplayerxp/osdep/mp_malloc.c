#include "mp_config.h"
#include "mplib.h"
#define MSGT_CLASS MSGT_OSDEP
#include "mp_msg.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <malloc.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <execinfo.h>

typedef struct mp_slot_s {
    any_t*	page_ptr;
    size_t	size;
    size_t	ncalls;
    any_t*	calls[10];
}mp_slot_t;

typedef struct priv_s {
    const char*			argv0;
    unsigned			rnd_limit;
    unsigned			every_nth_call;
    enum mp_malloc_e		flags;
    /* for randomizer */
    unsigned long long int	total_calls;
    /* backtrace and protector slots */
    mp_slot_t*			slots;
    size_t			nslots;
}priv_t;
static priv_t* priv;

static any_t* prot_page_align(any_t *ptr) { return (any_t*)(((unsigned long)ptr)&(~(__VM_PAGE_SIZE__-1))); }
static size_t prot_fullsize(size_t size) {
    unsigned npages = size/__VM_PAGE_SIZE__;
    unsigned fullsize;
    if(size%__VM_PAGE_SIZE__) npages++;
    npages++;
    fullsize=npages*__VM_PAGE_SIZE__;
    return fullsize;
}
static any_t* prot_last_page(any_t* rp,size_t fullsize) { return rp+(fullsize-__VM_PAGE_SIZE__); }
static void __prot_print_slots(void) {
    size_t i;
    for(i=0;i<priv->nslots;i++) {
	MSG_INFO("slot[%u] address: %p size: %u\n",i,priv->slots[i].page_ptr,priv->slots[i].size);
    }
}

static size_t	prot_find_slot_idx(any_t* ptr) {
    size_t i;
    for(i=0;i<priv->nslots;i++) {
	if(priv->slots[i].page_ptr==ptr) return i;
    }
    return UINT_MAX;
}

static mp_slot_t*	prot_find_slot(any_t* ptr) {
    size_t idx=prot_find_slot_idx(ptr);
    if(idx!=UINT_MAX) return &priv->slots[idx];
    return NULL;
}

static mp_slot_t*	prot_append_slot(any_t*ptr,size_t size) {
    mp_slot_t* slot;
    if(!priv->slots)	slot=malloc(sizeof(mp_slot_t));
    else		slot=realloc(priv->slots,sizeof(mp_slot_t)*(priv->nslots+1));
    priv->slots=slot;
    memset(&priv->slots[priv->nslots],0,sizeof(mp_slot_t));
    priv->slots[priv->nslots].page_ptr=ptr;
    priv->slots[priv->nslots].size=size;
    priv->nslots++;
    return &priv->slots[priv->nslots-1];
}

static void	prot_free_slot(any_t* ptr) {
    size_t idx=prot_find_slot_idx(ptr);
    if(idx!=UINT_MAX) {
	memmove(&priv->slots[idx],&priv->slots[idx+1],sizeof(mp_slot_t)*(priv->nslots-(idx+1)));
	priv->nslots--;
	priv->slots=realloc(priv->slots,sizeof(mp_slot_t)*priv->nslots);
    } else printf("Internal error! Can't find slot for address: %p\n",ptr);
}

static any_t* __prot_malloc(size_t size) {
    any_t* rp;
    size_t fullsize=prot_fullsize(size);
    rp=mp_memalign(__VM_PAGE_SIZE__,fullsize);
    if(rp) {
	prot_append_slot(rp,size);
	// protect last page here
	mprotect(prot_last_page(rp,fullsize),__VM_PAGE_SIZE__,MP_DENY_ALL);
	rp+=fullsize-__VM_PAGE_SIZE__-size;
    }
    return rp;
}

static any_t* __prot_memalign(size_t boundary,size_t size) { UNUSED(boundary);  return __prot_malloc(size); }

static void __prot_free(any_t*ptr) {
    any_t *page_ptr=prot_page_align(ptr);
    free(page_ptr);
    mp_slot_t* slot=prot_find_slot(page_ptr);
    if(!slot) {
	printf("Internal error! Can't find slot for address: %p\n",ptr);
	__prot_print_slots();
	kill(getpid(), SIGILL);
    }
    size_t fullsize=prot_fullsize(slot->size);
    mprotect(prot_last_page(page_ptr,fullsize),__VM_PAGE_SIZE__,MP_PROT_READ|MP_PROT_WRITE);
    prot_free_slot(ptr);
}

#define min(a,b) ((a)<(b)?(a):(b))
static any_t* __prot_realloc(any_t*ptr,size_t size) {
    any_t* rp;
    if((rp=__prot_malloc(size))!=NULL && ptr) {
	mp_slot_t* slot=prot_find_slot(prot_page_align(ptr));
	if(!slot) {
	    printf("Internal error! Can't find slot for address: %p\n",ptr);
	    __prot_print_slots();
	    kill(getpid(), SIGILL);
	}
	memcpy(rp,ptr,min(slot->size,size));
	__prot_free(ptr);
    }
    return rp;
}

static any_t* prot_malloc(size_t size) {
    any_t* rp;
    rp=__prot_malloc(size);
    return rp;
}

static any_t* prot_memalign(size_t boundary,size_t size) {
    any_t* rp;
    rp=__prot_memalign(boundary,size);
    return rp;
}

static any_t* prot_realloc(any_t*ptr,size_t size) {
    any_t* rp;
    rp=__prot_realloc(ptr,size);
    return rp;
}

static void prot_free(any_t*ptr) {
    __prot_free(ptr);
}

static __always_inline any_t* bt_malloc(size_t size) {
    any_t*rp;
    mp_slot_t* slot;
    rp=malloc(size);
    if(rp) {
	slot=prot_append_slot(rp,size);
	slot->ncalls=backtrace(slot->calls,10);
    }
    return rp;
}

static __always_inline any_t* bt_memalign(size_t boundary,size_t size) {
    any_t*rp;
    mp_slot_t* slot;
    rp=memalign(boundary,size);
    if(rp) {
	slot=prot_append_slot(rp,size);
	slot->ncalls=backtrace(slot->calls,10);
    }
    return rp;
}

static __always_inline any_t* bt_realloc(any_t*ptr,size_t size) {
    any_t* rp;
    mp_slot_t* slot;
    if(!ptr) return bt_malloc(size);
    rp=realloc(ptr,size);
    if(rp) {
	slot=prot_find_slot(ptr);
	if(!slot) MSG_WARN("BT Internal error! Can't find slot for address: %p\n",ptr);
	else {
	    slot->page_ptr=rp; // update address after realloc
	    slot->size=size;
	}
    }
    return rp;
}

static __always_inline void bt_free(any_t*ptr) {
    mp_slot_t* slot=prot_find_slot(ptr);
    if(!slot) MSG_WARN("BT Internal error! Can't find slot for address: %p\n",ptr);
    prot_free_slot(ptr);
    free(ptr);
}

/*======== STATISTICS =======*/
typedef struct bt_cache_entry_s {
    any_t*	addr;
    char*	str;
}bt_cache_entry_t;

typedef struct bt_cache_s {
    bt_cache_entry_t*	entry;
    unsigned		num_entries;
}bt_cache_t;

static bt_cache_t*	init_bt_cache(void) { return calloc(1,sizeof(bt_cache_t)); }

static void		uninit_bt_cache(bt_cache_t* cache) {
    unsigned i;
    for(i=0;i<cache->num_entries;i++) free(cache->entry[i].str);
    free(cache->entry);
    free(cache);
}

static char* bt_find_cache(bt_cache_t* cache,any_t* ptr) {
    unsigned i;
    for(i=0;i<cache->num_entries;i++) if(cache->entry[i].addr==ptr) return cache->entry[i].str;
    return NULL;
}

static bt_cache_entry_t* bt_append_cache(bt_cache_t* cache,any_t* ptr,const char *str) {
    if(!cache->entry)	cache->entry=malloc(sizeof(bt_cache_entry_t));
    else		cache->entry=realloc(cache->entry,sizeof(bt_cache_entry_t)*(cache->num_entries+1));
    cache->entry[cache->num_entries].addr=ptr;
    cache->entry[cache->num_entries].str=strdup(str);
    cache->num_entries++;
    return &cache->entry[cache->num_entries-1];
}

static char * addr2line(bt_cache_t* cache,any_t*ptr) {
    char *rs;
    if(priv->argv0) {
	bt_cache_entry_t* centry;
	unsigned i;
	char cmd[4096],result[4096];
	char ch;
	if((rs=bt_find_cache(cache,ptr))!=NULL) return rs;
	sprintf(cmd,"addr2line -s -e %s %p\n",priv->argv0,ptr);
	FILE* in=popen(cmd,"r");
	if(!in) return NULL;
	i=0;
	while(1) {
	    ch=fgetc(in);
	    if(feof(in)) break;
	    if(ch=='\n') break;
	    result[i++]=ch;
	    if(i>=sizeof(result)-1) break;
	}
	result[i]='\0';
	pclose(in);
	centry=bt_append_cache(cache,ptr,result);
	return centry->str;
    }
    return NULL;
}

static void bt_print_slots(void) {
    bt_cache_t* cache=init_bt_cache();
    size_t i,j;
    for(i=0;i<priv->nslots;i++) {
	char *s;
	int printable=1;
	MSG_INFO("address: %p size: %u dump: ",priv->slots[i].page_ptr,priv->slots[i].size);
	s=priv->slots[i].page_ptr;
	for(j=0;j<min(priv->slots[i].size,20);j++) {
	    if(!isprint(s[j])) {
		printable=0;
		break;
	    }
	}
	if(printable) MSG_INFO("%20s",s);
	else for(j=0;j<min(priv->slots[i].size,10);j++) {
	    MSG_INFO("%02X ",(unsigned char)s[j]);
	}
	MSG_INFO("\n");
	for(j=0;j<priv->slots[i].ncalls;j++) {
	    MSG_INFO("%s%p -> %s\n",j==0?"bt=>":"    ",priv->slots[i].calls[j],addr2line(cache,priv->slots[i].calls[j]));
	}
    }
    MSG_HINT("For source lines you may also print in (gdb): list *0xADDRESS\n");
    uninit_bt_cache(cache);
}
/* ================== HEAD FUNCTIONS  ======================= */
void	mp_init_malloc(const char *argv0,unsigned rnd_limit,unsigned every_nth_call,enum mp_malloc_e flags)
{
    if(!priv) priv=malloc(sizeof(priv_t));
    memset(priv,0,sizeof(priv_t));
    priv->argv0=argv0;
    priv->rnd_limit=rnd_limit;
    priv->every_nth_call=every_nth_call;
    priv->flags=flags;
}

void	mp_uninit_malloc(int verbose)
{
    if(priv->flags&MPA_FLG_BACKTRACE) {
	if(priv->nslots) {
	    MSG_WARN("Warning! %lli slots were not freed\n",priv->nslots);
	    if(verbose) bt_print_slots();
	}
    }
    free(priv);
    priv=NULL;
}

any_t* mp_malloc(size_t __size)
{
    any_t* rb,*rnd_buff=NULL;
    if(!priv) mp_init_malloc(NULL,1000,10,MPA_FLG_RANDOMIZER);
    if(priv->every_nth_call && priv->rnd_limit && !priv->flags) {
	if(priv->total_calls%priv->every_nth_call==0) {
	    rnd_buff=malloc(rand()%priv->rnd_limit);
	}
    }
    if(priv->flags&(MPA_FLG_BOUNDS_CHECK|MPA_FLG_BEFORE_CHECK)) rb=prot_malloc(__size);
    else if(priv->flags&MPA_FLG_BACKTRACE)			rb=bt_malloc(__size);
    else							rb=malloc(__size);
    if(rnd_buff) free(rnd_buff);
    priv->total_calls++;
    return rb;
}

/* randomizing of memalign is useless feature */
any_t*	mp_memalign (size_t boundary, size_t __size)
{
    any_t* rb;
    if(!priv) mp_init_malloc(NULL,1000,10,MPA_FLG_RANDOMIZER);
    if(priv->flags&(MPA_FLG_BOUNDS_CHECK|MPA_FLG_BEFORE_CHECK)) rb=prot_memalign(boundary,__size);
    else if(priv->flags&MPA_FLG_BACKTRACE)			rb=bt_memalign(boundary,__size);
    else							rb=memalign(boundary,__size);
    return rb;
}

any_t*	mp_realloc(any_t*__ptr, size_t __size) {
    any_t* rp;
    if(priv->flags&(MPA_FLG_BOUNDS_CHECK|MPA_FLG_BEFORE_CHECK)) rp=prot_realloc(__ptr,__size);
    else if(priv->flags&MPA_FLG_BACKTRACE)			rp=bt_realloc(__ptr,__size);
    else							rp=realloc(__ptr,__size);
    return rp;
}

void	mp_free(any_t*__ptr)
{
    if(!priv) mp_init_malloc(NULL,1000,10,MPA_FLG_RANDOMIZER);
    if(__ptr) {
	if(priv->flags&(MPA_FLG_BOUNDS_CHECK|MPA_FLG_BEFORE_CHECK))
	    prot_free(__ptr);
	else if(priv->flags&MPA_FLG_BACKTRACE)
	    bt_free(__ptr);
	else
	    free(__ptr);
    }
}

/* ================ APPENDIX ==================== */

any_t*	mp_mallocz (size_t __size) {
    any_t* rp;
    rp=mp_malloc(__size);
    if(rp) memset(rp,0,__size);
    return rp;
}

char *	mp_strdup(const char *src) {
    char *rs=NULL;
    if(src) {
	unsigned len=strlen(src);
	rs=mp_malloc(len+1);
	if(rs) strcpy(rs,src);
    }
    return rs;
}

int __FASTCALL__ mp_mprotect(const any_t* addr,size_t len,enum mp_prot_e flags)
{
    return mprotect(addr,len,flags);
}
