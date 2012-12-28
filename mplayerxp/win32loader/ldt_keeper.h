#ifndef LDT_KEEPER_H
#define LDT_KEEPER_H

#include "mpxp_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
  any_t* fs_seg;
  char* prev_struct;
  int fd;
} ldt_fs_t;

void Setup_FS_Segment(void);
ldt_fs_t* Setup_LDT_Keeper(void);
void Restore_LDT_Keeper(ldt_fs_t* ldt_fs);
#ifdef __cplusplus
}
#endif

#endif /* LDT_KEEPER_H */
