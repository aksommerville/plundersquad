#include "ps.h"
#include "ps_linebreaker.h"

/* Cleanup.
 */

void ps_linebreaker_cleanup(struct ps_linebreaker *lb) {
  if (!lb) return;
  if (lb->v) free(lb->v);
  memset(lb,0,sizeof(struct ps_linebreaker));
}

/* Create a new line.
 */

static struct ps_linebreaker_line *ps_linebreaker_next(struct ps_linebreaker *lb) {
  if (lb->c>=lb->a) {
    int na=lb->a+8;
    if (na>INT_MAX/sizeof(struct ps_linebreaker_line)) return 0;
    void *nv=realloc(lb->v,sizeof(struct ps_linebreaker_line)*na);
    if (!nv) return 0;
    lb->v=nv;
    lb->a=na;
  }
  return lb->v+lb->c++;
}

/* Which characters are space?
 */

static inline int ps_nonspace(char ch) {
  return ((ch>=0x21)&&(ch<=0x7e));
}

/* Break lines.
 */
 
int ps_linebreaker_rebuild(
  struct ps_linebreaker *lb,
  const char *src,int srcc,
  int glyphw,
  int wlimit
) {
  if (!lb) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (glyphw<1) return -1;
  lb->c=0;

  // Available width, in characters.
  int chlimit=wlimit/glyphw;
  if (chlimit<1) chlimit=1;

  int srcp=0;
  while (srcp<srcc) {

    /* Allocate a line and set an optimistic guess at its length. */
    struct ps_linebreaker_line *line=ps_linebreaker_next(lb);
    if (!line) return -1;
    line->p=srcp;
    line->c=chlimit;

    /* If it reaches end of input, truncate. */
    if (line->p>=srcc-line->c) {
      line->c=srcc-line->p;
    }

    /* If there's an explicit newline, consume it and stop there. 
     * Do no more processing, because we don't want to consume whitespace from the next line (ie indentation).
     */
    int hard_stop=0;
    int i; for (i=0;i<line->c;i++) {
      if ((src[line->p+i]==0x0a)||(src[line->p+i]==0x0d)) {
        // Allow CRLF and LFCR:
        int nextp=line->p+i+1;
        if (nextp<line->c) {
          char nextch=src[nextp];
          if ((nextch!=src[line->p+i])&&((nextch==0x0a)||(nextch==0x0d))) {
            i++;
          }
        }
        line->c=i+1;
        hard_stop=1;
      }
    }
    if (hard_stop) {
      srcp+=line->c;
      continue;
    }

    /* If the last character of this line and the next one are both nonspace, we need to back up to a breaking point.
     */
    if ((line->p+line->c<srcc)&&ps_nonspace(src[line->p+line->c-1])&&ps_nonspace(src[line->p+line->c])) {
      int c0=line->c;
      while ((line->c>0)&&ps_nonspace(src[line->p+line->c-1])) line->c--;
      /* If backing up caused us to drop the whole line, fuck it. Break the word. */
      if (!line->c) line->c=c0;
    }

    /* Consume additional spaces, they are assumed not to print and therefore can overrun the margin.
     * If there are newlines in the additional space, we must consume only one.
     */
    while ((line->p+line->c<srcc)&&(src[line->p+line->c]<=0x20)) {
      if ((src[line->p+line->c]==0x0a)||(src[line->p+line->c]==0x0d)) {
        line->c++;
        if (line->p+line->c<srcc) {
          char nextch=src[line->p+line->c];
          if ((nextch!=src[line->p+line->c-1])&&((nextch==0x0a)||(nextch==0x0d))) line->c++;
        }
        break;
      }
      line->c++;
    }

    srcp+=line->c;
  }

  return 0;
}
