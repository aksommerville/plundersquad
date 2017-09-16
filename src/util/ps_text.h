/* ps_text.h
 * Helpers for raw text.
 */

#ifndef PS_TEXT_H
#define PS_TEXT_H

#define PS_CH_ISDIGIT(ch) (((ch)>=0x30)&&((ch)<=0x39))
#define PS_CH_ISUPPER(ch) (((ch)>=0x41)&&((ch)<=0x5a))
#define PS_CH_ISLOWER(ch) (((ch)>=0x61)&&((ch)<=0x7a))
#define PS_CH_ISEXTRA(ch) ((ch)==0x5f)
#define PS_CH_ISALPHA(ch) (PS_CH_ISUPPER(ch)||PS_CH_ISLOWER(ch))
#define PS_CH_ISIDENT(ch) (PS_CH_ISDIGIT(ch)||PS_CH_ISALPHA(ch)||PS_CH_ISEXTRA(ch))
#define PS_CH_ISQUOTE(ch) (((ch)==0x22)||((ch)==0x27)||((ch)==0x60))

int ps_newlines_count(const char *src,int srcc);
int ps_line_measure(const char *src,int srcc);
int ps_memcasecmp(const void *a,const void *b,int c);
int ps_strcpy(char *dst,int dsta,const char *src,int srcc);
int ps_space_measure(const char *src,int srcc);

int ps_utf8_encode(void *dst,int dsta,int src);
int ps_utf8_decode(int *dst,const void *src,int srcc);

static inline int ps_hexdigit_eval(char ch) {
  if ((ch>='0')&&(ch<='9')) return ch-'0';
  if ((ch>='a')&&(ch<='f')) return ch-'a'+10;
  if ((ch>='A')&&(ch<='F')) return ch-'A'+10;
  return -1;
}

static inline char ps_hexdigit_repr(int src) {
  return "0123456789abcdef"[src&15];
}

/* Evaluate integers.
 * Signed or unsigned, decimal or hexadecimal.
 * Evaluators return 0 if a positive value appears negative.
 * The 'interactive' version adds a range check and logging. ("TEXT" domain).
 */
int ps_int_measure(const char *src,int srcc);
int ps_int_eval(int *dst,const char *src,int srcc);
int ps_int_eval_interactive(int *dst,const char *src,int srcc,int lo,int hi,const char *name);

int ps_decsint_repr(char *dst,int dsta,int src);
int ps_hexuint_repr(char *dst,int dsta,int src);

/* Nothing too shocking in our string format, it's like C.
 * There are no octal escapes.
 * '#' is escaped as '\H', since we often use it as a line comment.
 */
int ps_str_measure(const char *src,int srcc);
int ps_str_eval(char *dst,int dsta,const char *src,int srcc);
int ps_str_repr(char *dst,int dsta,const char *src,int srcc);

/* Match a string against a wildcard pattern.
 * Letters are case-insensitive.
 * Whitespace condenses; leading and trailing is trimmed.
 * '*' matches any amount of anything.
 * '\' escapes one character, forcing precise (case-sensitive) comparison.
 * The score of a match is one plus the count of non-whitespace characters matched without wildcard.
 */
int ps_pattern_match(const char *pat,int patc,const char *src,int srcc);

struct ps_line_reader {
  const char *src;
  int srcc;
  int srcp;
  int enable_comments;
  int lineno;
  const char *line;
  int linec;
};

/* Initialize a line reader.
 * Caller must ensure that (src) remain valid and constant throughout line reader's life.
 * No cleanup is necessary.
 * If (enable_comments), anything from '#' to end of line is ignored.
 * Enabling comments also instructs the reader to trim leading and trailing whitespace.
 * You can toggle that flag at any time.
 * A fresh line reader has no line, you must call ps_line_reader_next() to queue up the first one.
 */
int ps_line_reader_begin(struct ps_line_reader *reader,const char *src,int srcc,int enable_comments);

/* Advance reader's position to the next line of text.
 * Returns 0 at end of text, >0 if a line is ready, or <0 if reader is inconsistent.
 * After a positive response, (reader->lineno,reader->line,reader->linec) all refer to the next line.
 * Newlines are never included in the text we report.
 */
int ps_line_reader_next(struct ps_line_reader *reader);

#endif
