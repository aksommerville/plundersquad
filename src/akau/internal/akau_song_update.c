#include "akau_song_internal.h"
#include "../akau_mixer.h"

/* Update single command.
 * Return >0 to finish update, <0 on error, or 0 to proceed.
 */

static int akau_song_execute_command(struct akau_song *song,const union akau_song_command *cmd,struct akau_mixer *mixer,uint8_t intent) {
  switch (cmd->op) {

    case AKAU_SONG_OP_NOOP: return 0;

    case AKAU_SONG_OP_BEAT: {
        if (song->cb_sync) {
          if (song->cb_sync(song,song->beatp,song->userdata)<0) return -1;
        }
        song->beatp++;
        if (akau_mixer_set_song_delay(mixer,song->frames_per_beat)<0) return -1;
      } return 1;

    case AKAU_SONG_OP_NOTE: {
        if (cmd->NOTE.instrid>=song->instrc) return -1;
        struct akau_instrument *instrument=song->instrv[cmd->NOTE.instrid].instrument;
        int err=akau_mixer_play_note(mixer,instrument,cmd->NOTE.pitch,cmd->NOTE.trim,cmd->NOTE.pan,cmd->NOTE.duration*song->frames_per_beat,intent);
        if (err<0) return -1;
        song->chanid_ref[cmd->NOTE.ref]=err;
      } return 0;

    case AKAU_SONG_OP_DRUM: {
        if (cmd->DRUM.drumid>=song->drumc) return -1;
        struct akau_ipcm *ipcm=song->drumv[cmd->DRUM.drumid].ipcm;
        int err=akau_mixer_play_ipcm(mixer,ipcm,cmd->DRUM.trim,cmd->DRUM.pan,0,intent);
        if (err<0) return -1;
        song->chanid_ref[cmd->DRUM.ref]=err;
      } return 0;

    case AKAU_SONG_OP_ADJPITCH: {
        int chanid=song->chanid_ref[cmd->ADJ.ref];
        uint8_t pitch,trim;
        int8_t pan;
        if (akau_mixer_get_channel(&pitch,&trim,&pan,mixer,chanid)<0) return -1;
        if (akau_mixer_adjust_channel(mixer,chanid,cmd->ADJ.v,trim,pan,cmd->ADJ.duration*song->frames_per_beat)<0) return -1;
      } return 0;

    case AKAU_SONG_OP_ADJTRIM: {
        int chanid=song->chanid_ref[cmd->ADJ.ref];
        uint8_t pitch,trim;
        int8_t pan;
        if (akau_mixer_get_channel(&pitch,&trim,&pan,mixer,chanid)<0) return -1;
        if (akau_mixer_adjust_channel(mixer,chanid,pitch,cmd->ADJ.v,pan,cmd->ADJ.duration*song->frames_per_beat)<0) return -1;
      } return 0;

    case AKAU_SONG_OP_ADJPAN: {
        int chanid=song->chanid_ref[cmd->ADJ.ref];
        uint8_t pitch,trim;
        int8_t pan;
        if (akau_mixer_get_channel(&pitch,&trim,&pan,mixer,chanid)<0) return -1;
        if (akau_mixer_adjust_channel(mixer,chanid,pitch,trim,cmd->ADJ.v,cmd->ADJ.duration*song->frames_per_beat)<0) return -1;
      } return 0;
      
  }
  return -1;
}

/* Update.
 */

int akau_song_update(struct akau_song *song,struct akau_mixer *mixer,int cmdp,uint8_t intent) {
  if (!song||!mixer) return -1;
  if (song->cmdc<1) return -1;
  int reset=0;
  if ((cmdp<0)||(cmdp>=song->cmdc)) {
    song->beatp=0;
    cmdp=0;
  }

  while (cmdp<song->cmdc) {
    const union akau_song_command *cmd=song->cmdv+cmdp;
    int err=akau_song_execute_command(song,cmd,mixer,intent);
    if (err<0) return -1;
    cmdp++;
    if (err>0) break;
  }

  if (akau_mixer_set_song_position(mixer,cmdp)<0) return -1;
  return 0;
}
