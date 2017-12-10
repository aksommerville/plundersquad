/* ps_iwg.h
 * Intermediate Wave Generator.
 * This is closely tied to akgu_wavegen.
 * It presents a mutable data model for use by the editor.
 * We can convert to and from the portable text format, and from there to akau_wavegen.
 */

#ifndef PS_IWG_H
#define PS_IWG_H

struct akau_wavegen_decoder;
struct akau_ipcm;

struct ps_iwg_channel {
  char *arg;
  int argc;
};

struct ps_iwg_command {
  int time; // milliseconds
  uint8_t chanid;
  uint8_t k; // AKAU_WAVEGEN_K_*
  double v;
};

struct ps_iwg {
  int refc;
  struct ps_iwg_channel *chanv;
  int chanc,chana;
  struct ps_iwg_command *cmdv;
  int cmdc,cmda;
  int dirty; // manual use only, we never touch it
};

void ps_iwg_channel_cleanup(struct ps_iwg_channel *chan);

struct ps_iwg *ps_iwg_new();
void ps_iwg_del(struct ps_iwg *iwg);
int ps_iwg_ref(struct ps_iwg *iwg);

int ps_iwg_clear(struct ps_iwg *iwg);

/* Add a channel or command.
 * For channels, we only validate and record the text to initialize it.
 */
int ps_iwg_add_channel(struct ps_iwg *iwg,const char *arg,int argc);
int ps_iwg_add_command(struct ps_iwg *iwg,int time_ms,int chanid,int k,double v);

/* Remove one channel definition.
 * Also removes all commands on that channel, and adjust chanid in other channels as necessary.
 */
int ps_iwg_remove_channel(struct ps_iwg *iwg,int chanid);

/* Replace the channel's definition as plain text.
 * We don't validate, so please be careful.
 */
int ps_iwg_set_channel_shape(struct ps_iwg *iwg,int chanid,const char *arg,int argc);

/* Remove a range of comamnds.
 */
int ps_iwg_remove_commands(struct ps_iwg *iwg,int p,int c);

/* Commands must remain sorted by time.
 * Use this function to change a command's time; it shuffles as necessary to maintain order.
 */
int ps_iwg_adjust_command_time(struct ps_iwg *iwg,int p,int new_time_ms);

/* Return the count of commands at this exact time, and the first position or insertion point.
 */
int ps_iwg_search_command_time(int *cmdp,const struct ps_iwg *iwg,int time_ms);

/* Return the position of the next command >= (p) matching (chanid,k).
 * "reverse" returns the next <= (p).
 */
int ps_iwg_search_command_field(const struct ps_iwg *iwg,int p,int chanid,int k);
int ps_iwg_search_command_field_reverse(const struct ps_iwg *iwg,int p,int chanid,int k);

const char *ps_iwg_k_repr(int k);

/* Primitive encode and decode.
 * This uses exactly the same format as akau_wavegen; it is documented at <akau/akau_wavegen.h>.
 * Decoding wipes the object's state first.
 * It is perfectly reasonable to do multiple encodes and decodes on the same object, as you change it.
 */
int ps_iwg_encode(void *dstpp,const struct ps_iwg *iwg);
int ps_iwg_decode(struct ps_iwg *iwg,const char *src,int srcc);

/* Convenience encode and decode.
 * _to_wavegen and _to_ipcm serialize as an intermediate step even though it's not strictly necessary.
 */
struct akau_wavegen_decoder *ps_iwg_to_wavegen_decoder(const struct ps_iwg *iwg);
struct akau_ipcm *ps_iwg_to_ipcm(const struct ps_iwg *iwg);
int ps_iwg_write_file(const char *path,const struct ps_iwg *iwg);
int ps_iwg_read_file(struct ps_iwg *iwg,const char *path);

#endif
