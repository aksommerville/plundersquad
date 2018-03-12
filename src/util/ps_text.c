#include "ps.h"
#include "ps_text.h"

/* Count newlines.
 */

int ps_newlines_count(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,nlc=0;
  while (srcp<srcc) {
    if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      nlc++;
      if (++srcp>=srcc) return nlc;
      if ((src[srcp]!=src[srcp-1])&&((src[srcp]==0x0a)||(src[srcp]==0x0d))) srcp++;
    } else {
      srcp++;
    }
  }
  return nlc;
}

/* Measure line.
 */
 
int ps_line_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while (srcp<srcc) {
    if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      if (++srcp>=srcc) return srcp;
      if ((src[srcp]!=src[srcp-1])&&((src[srcp]==0x0a)||(src[srcp]==0x0d))) srcp++;
      return srcp;
    }
    srcp++;
  }
  return srcp;
}

/* Case-insensitive memcmp.
 */
 
int ps_memcasecmp(const void *a,const void *b,int c) {
  if (c<1) return 0;
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  while (c-->0) {
    uint8_t cha=*(uint8_t*)a; a=(char*)a+1;
    uint8_t chb=*(uint8_t*)b; b=(char*)b+1;
    if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* String copy.
 */
 
int ps_strcpy(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

/* Measure whitespace.
 */
 
int ps_space_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]>0x20) return srcp;
  }
  return srcp;
}

/* Assert G0 only.
 */
 
int ps_is_g0(const char *src,int srcc) {
  if (!src) return 1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc-->0) {
    if (*src<0x20) return 0;
    if (*src>0x7e) return 0;
    src++;
  }
  return 1;
}

/* Encode UTF-8.
 */

int ps_utf8_encode(void *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  uint8_t *DST=dst;
  if (src<0) return -1;
  if (src<0x80) {
    if (dsta>=1) {
      DST[0]=src;
    }
    return 1;
  }
  if (src<0x800) {
    if (dsta>=2) {
      DST[0]=0xc0|(src>>6);
      DST[1]=0x80|(src&0x3f);
    }
    return 2;
  }
  if (src<0x10000) {
    if (dsta>=3) {
      DST[0]=0xe0|(src>>12);
      DST[1]=0x80|((src>>6)&0x3f);
      DST[2]=0x80|(src&0x3f);
    }
    return 3;
  }
  if (src<0x110000) {
    if (dsta>=4) {
      DST[0]=0xf0|(src>>18);
      DST[1]=0x80|((src>>12)&0x3f);
      DST[2]=0x80|((src>>6)&0x3f);
      DST[3]=0x80|(src&0x3f);
    }
    return 4;
  }
  return -1;
}

/* Decode UTF-8.
 */
 
int ps_utf8_decode(int *dst,const void *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) srcc=4;
  const uint8_t *SRC=src;
  *dst=SRC[0];
  if (!(*dst&0x80)) return 1;
  if (!(*dst&0x40)) return -1;
  if (!(*dst&0x20)) {
    if (srcc<2) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    *dst=((*dst&0x3f)<<6)|(SRC[1]&0x3f);
    return 2;
  }
  if (!(*dst&0x10)) {
    if (srcc<3) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    *dst=((*dst&0x3f)<<12)|((SRC[1]&0x3f)<<6)|(SRC[2]&0x3f);
    return 3;
  }
  if (!(*dst&0x08)) {
    if (srcc<4) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    if ((SRC[3]&0xc0)!=0x80) return -1;
    *dst=((*dst&0x3f)<<18)|((SRC[1]&0x3f)<<12)|((SRC[2]&0x3f)<<6)|(SRC[3]&0x3f);
    return 4;
  }
  return -1;
}

/* Measure integer.
 */

int ps_int_measure(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return -1;

  int srcp=0;
  if ((src[0]=='-')||(src[0]=='+')) {
    if (srcc<1) return -1;
    srcp=1;
  }

  if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
  srcp++;
  while ((srcp<srcc)&&PS_CH_ISIDENT(src[srcp])) srcp++;

  return srcp;
}

/* Evaluate integer.
 */

int ps_int_eval(int *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  int srcp=0,positive=1,base=10,overflow=0;
  
  if (src[0]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[0]=='+') {
    if (++srcp>=srcc) return -1;
  }

  if ((srcp<=srcc-3)&&(src[srcp]=='0')&&((src[srcp+1]=='x')||(src[srcp+1]=='X'))) {
    base=16;
    srcp+=2;
  }

  int limit;
  if (positive) limit=UINT_MAX/base;
  else limit=INT_MIN/base;
  
  *dst=0;
  while (srcp<srcc) {
    int digit=src[srcp++];
         if ((digit>='0')&&(digit<='9')) digit=digit-'0';
    else if ((digit>='a')&&(digit<='z')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='Z')) digit=digit-'A'+10;
    else return -1;
    if (digit>=base) return -1;
    if (positive) {
      if ((unsigned int)*dst>limit) overflow=1;
      *dst*=base;
      if ((unsigned int)*dst>UINT_MAX-digit) overflow=1;
      *dst+=digit;
    } else {
      if (*dst<limit) overflow=1;
      *dst*=base;
      if (*dst<INT_MIN+digit) overflow=1;
      *dst-=digit;
    }
  }

  if (overflow) return -1;
  if (positive&&(*dst<0)) return 0;
  return 1;
}

/* Evaluate with logging and limits.
 */
 
int ps_int_eval_interactive(int *dst,const char *src,int srcc,int lo,int hi,const char *name) {
  if (!dst) return -1;
  if (!name||!name[0]) name="integer";
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1,base=10,overflow=0;

  if (srcp<srcc) {
    if (src[0]=='-') {
      positive=0;
      srcp++;
    } else if (src[0]=='+') {
      srcp++;
    }
  }
  if (srcp>=srcc) {
    ps_log(TEXT,ERROR,"Incomplete integer '%.*s' for %s.",srcc,src,name);
    return -1;
  }

  if ((srcp<=srcc-3)&&(src[srcp]=='0')&&((src[srcp+1]=='x')||(src[srcp+1]=='X'))) {
    base=16;
    srcp+=2;
  }

  int limit;
  if (positive) limit=UINT_MAX/base;
  else limit=INT_MIN/base;
  
  *dst=0;
  while (srcp<srcc) {
    int digit=src[srcp++];
         if ((digit>='0')&&(digit<='9')) digit=digit-'0';
    else if ((digit>='a')&&(digit<='z')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='Z')) digit=digit-'A'+10;
    else {
      ps_log(TEXT,ERROR,"Unexpected character '%c' in %s.",digit,name);
      return -1;
    }
    if (digit>=base) {
      ps_log(TEXT,ERROR,"Illegal character '%c' in base-%d integer for %s.",digit,base,name);
      return -1;
    }
    if (positive) {
      if ((unsigned int)*dst>limit) overflow=1;
      *dst*=base;
      if ((unsigned int)*dst>UINT_MAX-digit) overflow=1;
      *dst+=digit;
    } else {
      if (*dst<limit) overflow=1;
      *dst*=base;
      if (*dst<INT_MIN+digit) overflow=1;
      *dst-=digit;
    }
  }

  if (overflow) {
    ps_log(TEXT,ERROR,"Integer overflow in %s.",name);
    return -1;
  }
  if ((*dst<lo)||(*dst>hi)) {
    ps_log(TEXT,ERROR,"Illegal value %d for %s, limit %d..%d.",*dst,name,lo,hi);
    return -1;
  }
  if (positive&&(*dst<0)) return 0;
  return 1;
}

/* Represent decimal integer.
 */

int ps_decsint_repr(char *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  int dstc,i,limit;
  if (src<0) {
    dstc=2;
    limit=-10;
    while (limit>=src) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->1;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
  } else {
    dstc=1;
    limit=10;
    while (limit<=src) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->0;src/=10) dst[i]='0'+src%10;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Represent hexadecimal unsigned integer.
 */
 
int ps_hexuint_repr(char *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  int dstc=3;
  unsigned int limit=0x10;
  while ((unsigned int)src>=limit) { dstc++; if (src&~(UINT_MAX>>4)) break; limit<<=4; }
  if (dstc>dsta) return dstc;
  dst[0]='0';
  dst[1]='x';
  int i; for (i=dstc;i-->2;src>>=4) dst[i]=ps_hexdigit_repr(src);
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Evaluate floating-point number.
 */
 
int ps_double_eval(double *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1;
  
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return -1;
    positive=0;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }

  if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
  *dst=0.0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    *dst*=10.0;
    *dst+=digit;
  }

  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    double coef=1.0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      int digit=src[srcp++]-'0';
      coef*=0.1;
      *dst+=digit*coef;
    }
  }

  if (srcp<srcc) return -1;
  if (!positive) *dst=-*dst;
  return 0;
}

/* Measure string token.
 */

int ps_str_measure(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  if (!PS_CH_ISQUOTE(src[0])) return -1;
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return -1;
    if (src[srcp]==src[0]) return srcp+1;
    if (src[srcp]=='\\') srcp+=2;
    else srcp+=1;
  }
}

/* Evaluate string.
 */
 
int ps_str_eval(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<2)||!PS_CH_ISQUOTE(src[0])||(src[0]!=src[srcc-1])) return -1;
  src++; srcc-=2;
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='\\') {
      if (++srcp>=srcc) return -1;
      switch (src[srcp++]) {
        case '\\': case '\'': case '"': case '`': if (dstc<dsta) dst[dstc]=src[srcp-1]; dstc++; break;
        case '0': if (dstc<dsta) dst[dstc]=0x00; dstc++; break;
        case 't': if (dstc<dsta) dst[dstc]=0x09; dstc++; break;
        case 'n': if (dstc<dsta) dst[dstc]=0x0a; dstc++; break;
        case 'r': if (dstc<dsta) dst[dstc]=0x0d; dstc++; break;
        case 'H': if (dstc<dsta) dst[dstc]='#'; dstc++; break;
        case 'x': {
            if (srcp>srcc-2) return -1;
            int hi=ps_hexdigit_eval(src[srcp++]);
            int lo=ps_hexdigit_eval(src[srcp++]);
            if ((hi<0)||(lo<hi)) return -1;
            if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
            dstc++;
          } break;
        case 'u': {
            int ch=0,digitc=0;
            while ((srcp<srcc)&&(digitc<6)) {
              int digit=ps_hexdigit_eval(src[srcp]);
              if (digit<0) break;
              ch<<=4;
              ch|=digit;
              srcp++;
              digitc++;
            }
            if (!digitc) return -1;
            int err=ps_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (err<0) return -1;
            dstc+=err;
          } break;
        default: return -1;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Represent string.
 */

int ps_str_repr(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  if (dstc<dsta) dst[dstc]='"'; dstc++;
  while (srcp<srcc) {
    switch (src[srcp]) {
      case '"': case '\\': if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]=src[srcp]; } dstc+=2; srcp++; break;
      case 0x00: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='0'; } dstc+=2; srcp++; break;
      case 0x09: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='t'; } dstc+=2; srcp++; break;
      case 0x0a: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='n'; } dstc+=2; srcp++; break;
      case 0x0d: if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='r'; } dstc+=2; srcp++; break;
      case '#': if (dstc<=dsta-2) { dst[dstc]='\\'; dst[dstc+1]='H'; } dstc+=2; srcp++; break;
      default: if ((src[srcp]>=0x20)&&(src[srcp]<=0x7e)) {
          if (dstc<dsta) dst[dstc]=src[srcp];
          dstc++;
          srcp++;
        } else {
          if (dstc<=dsta-4) {
            dst[dstc+0]='\\';
            dst[dstc+1]='x';
            dst[dstc+2]=ps_hexdigit_repr(src[srcp]>>4);
            dst[dstc+3]=ps_hexdigit_repr(src[srcp]);
          }
          dstc+=4;
          srcp++;
        }
    }
  }
  if (dstc<dsta) dst[dstc]='"'; dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Match pattern.
 */
 
int ps_pattern_match(const char *pat,int patc,const char *src,int srcc) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  /* Leading and trailing whitespace. */
  while (patc&&((unsigned char)pat[patc-1]<=0x20)) patc--;
  while (patc&&((unsigned char)pat[0]<=0x20)) { pat++; patc--; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  
  int patp=0,srcp=0,score=1;
  while (1) {

    /* End of pattern: terminate. */
    if (patp>=patc) {
      if (srcp>=srcc) return score;
      return 0;
    }

    /* Wildcard. */
    if (pat[patp]=='*') {
      patp++;
      if (patp>=patc) return score;
      while (srcp<srcc) {
        int subscore=ps_pattern_match(pat+patp,patc-patp,src+srcp,srcc-srcp);
        if (subscore>0) {
          if (score>INT_MAX-subscore) return INT_MAX;
          return score+subscore;
        }
      }
      return 0;
    }

    /* End of input: failure. */
    if (srcp>=srcc) return 0;

    /* Literal character. */
    if (pat[patp]=='\\') {
      if (++patp>=patc) return 0; // Malformed pattern.
      if (pat[patp]!=src[srcp]) return 0;
      patp++;
      srcp++;
      if (score<INT_MAX) score++;
      continue;
    }

    /* Whitespace. */
    if ((unsigned char)pat[patp]<=0x20) {
      if ((unsigned char)src[srcp]>0x20) return 0;
      patp++; while ((patp<patc)&&((unsigned char)pat[patp]<=0x20)) patp++;
      srcp++; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      continue;
    }

    /* Regular characters. */
    char a=pat[patp++]; if ((a>=0x41)&&(a<=0x5a)) a+=0x20;
    char b=src[srcp++]; if ((b>=0x41)&&(b<=0x5a)) b+=0x20;
    if (a!=b) return 0;
    if (score<INT_MAX) score++;

  }
  return 0;
}

/* Initialize line reader.
 */

int ps_line_reader_begin(struct ps_line_reader *reader,const char *src,int srcc,int enable_comments) {
  if (!reader) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  memset(reader,0,sizeof(struct ps_line_reader));
  reader->src=src;
  reader->srcc=srcc;
  reader->enable_comments=enable_comments;
  return 0;
}

/* Advance line reader.
 */
 
int ps_line_reader_next(struct ps_line_reader *reader) {
  if (!reader) return -1;
  if (reader->srcp<0) return -1;
  if (reader->srcp>=reader->srcc) return 0;
  if (!reader->src) return -1;

  reader->lineno++;
  reader->line=reader->src+reader->srcp;
  reader->linec=0;
  int comment=0;

  while (reader->srcp<reader->srcc) {

    /* Pop the next character. */
    char ch=reader->src[reader->srcp++];

    /* Stop reading if newline (LF,CR,LFCR,CRLF). */
    if ((ch==0x0a)||(ch==0x0d)) {
      if ((reader->srcp<reader->srcc)&&(reader->src[reader->srcp]!=ch)) {
        if ((reader->src[reader->srcp]==0x0a)||(reader->src[reader->srcp]==0x0d)) {
          reader->srcp++;
        }
      }
      break;
    }

    /* Check for comments if enabled. */
    if (reader->enable_comments&&(ch=='#')) comment=1;

    /* Add to the current line if we're not commenting. */
    if (!comment) reader->linec++;
  }

  /* If comments are enabled, trim whitespace too. */
  if (reader->enable_comments) {
    while (reader->linec&&((unsigned char)reader->line[reader->linec-1]<=0x20)) reader->linec--;
    while (reader->linec&&((unsigned char)reader->line[0]<=0x20)) { reader->line++; reader->linec--; }
  }
  
  return 1;
}
