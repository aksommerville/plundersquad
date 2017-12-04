/* ps_linebreaker.h
 * Structured object for breaking lines of text and recording the line starts.
 */

#ifndef PS_LINEBREAKER_H
#define PS_LINEBREAKER_H

struct ps_linebreaker {
  struct ps_linebreaker_line {
    int p,c;
  } *v;
  int c,a;
};

void ps_linebreaker_cleanup(struct ps_linebreaker *linebreaker);

int ps_linebreaker_rebuild(
  struct ps_linebreaker *linebreaker,
  const char *src,int srcc,
  int glyphw,
  int wlimit
);

#endif
