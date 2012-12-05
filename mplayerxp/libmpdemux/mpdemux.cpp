#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdlib.h>

#include "input2/input.h"
#include "mpdemux.h"
#include "demux_msg.h"

int mpdemux_check_interrupt(libinput_t* libinput,int _time) {
  mp_cmd_t* cmd;
  if((cmd = mp_input_get_cmd(libinput,_time,0,1)) == NULL)
    return 0;

  switch(cmd->id) {
  case MP_CMD_QUIT:
  case MP_CMD_SOFT_QUIT:
  case MP_CMD_PLAY_TREE_STEP:
  case MP_CMD_PLAY_TREE_UP_STEP:
  case MP_CMD_PLAY_ALT_SRC_STEP:
    // The cmd will be executed when we are back in the main loop
    return 1;
  default:
    // remove the cmd from then queue
    cmd = mp_input_get_cmd(libinput,_time,0,0);
    mp_cmd_free(cmd);
    return 0;
  }
}


