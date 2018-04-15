#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_userconfig.h"
#include "os/ps_fs.h"
#include "os/ps_file_list.h"
#include "util/ps_buffer.h"
#include "util/ps_text.h"
#include <sys/stat.h>

/* Private definitions.
 */

struct ps_userconfig_field {
  int type;
  char *k;
  int kc;
  union {
    struct {
      int v;
    } b;
    struct {
      int v;
      int lo,hi;
    } i;
    struct {
      char *v;
      int c;
    } s;
    struct {
      char *v;
      int c;
      int must_exist;
    } p;
  };
};

struct ps_userconfig {
  struct ps_userconfig_field *fldv;
  int fldc,flda;
  char *path;
  int refc;
  int dirty;
  struct ps_file_list *config_file_list;
  struct ps_file_list *input_file_list;
  struct ps_file_list *data_file_list;
};

/* Single-field primitives.
 */

static void ps_userconfig_field_cleanup(struct ps_userconfig_field *field) {
  if (field->k) free(field->k);
  switch (field->type) {
    case PS_USERCONFIG_TYPE_STRING: {
        if (field->s.v) free(field->s.v);
      } break;
    case PS_USERCONFIG_TYPE_PATH: {
        if (field->p.v) free(field->p.v);
      } break;
  }
}

/* Object lifecycle.
 */

struct ps_userconfig *ps_userconfig_new() {
  struct ps_userconfig *userconfig=calloc(1,sizeof(struct ps_userconfig));
  if (!userconfig) return 0;

  userconfig->refc=1;

  if (
    !(userconfig->config_file_list=ps_file_list_new())||
    !(userconfig->input_file_list=ps_file_list_new())||
    !(userconfig->data_file_list=ps_file_list_new())
  ) {
    ps_userconfig_del(userconfig);
    return 0;
  }

  return userconfig;
}

void ps_userconfig_del(struct ps_userconfig *userconfig) {
  if (!userconfig) return;
  if (userconfig->refc-->1) return;

  if (userconfig->path) free(userconfig->path);
  ps_file_list_del(userconfig->config_file_list);
  ps_file_list_del(userconfig->input_file_list);
  ps_file_list_del(userconfig->data_file_list);

  if (userconfig->fldv) {
    while (userconfig->fldc-->0) ps_userconfig_field_cleanup(userconfig->fldv+userconfig->fldc);
    free(userconfig->fldv);
  }

  free(userconfig);
}

int ps_userconfig_ref(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  if (userconfig->refc<1) return -1;
  if (userconfig->refc==INT_MAX) return -1;
  userconfig->refc++;
  return 0;
}

/* Declare default fields.
 */
 
int ps_userconfig_declare_default_fields(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  #define BOOLEAN(k,v) if (ps_userconfig_declare_boolean(userconfig,k,v)<0) return -1;
  #define INTEGER(k,v,lo,hi) if (ps_userconfig_declare_integer(userconfig,k,v,lo,hi)<0) return -1;
  #define STRING(k,v) if (ps_userconfig_declare_string(userconfig,k,v,sizeof(v)-1)<0) return -1;
  #define PATH(k,v) if (ps_userconfig_declare_path(userconfig,k,v,sizeof(v)-1,0)<0) return -1;

  PATH("resources","")
  PATH("input","")
  BOOLEAN("fullscreen",1)
  BOOLEAN("soft-render",0)
  INTEGER("music",255,0,255)
  INTEGER("sound",255,0,255)

  #undef BOOLEAN
  #undef INTEGER
  #undef STRING
  #undef PATH  
  return 0;
}

/* File lists.
 */

struct ps_file_list *ps_userconfig_get_config_file_list(const struct ps_userconfig *userconfig) {
  if (!userconfig) return 0;
  return userconfig->config_file_list;
}

struct ps_file_list *ps_userconfig_get_input_file_list(const struct ps_userconfig *userconfig) {
  if (!userconfig) return 0;
  return userconfig->input_file_list;
}

struct ps_file_list *ps_userconfig_get_data_file_list(const struct ps_userconfig *userconfig) {
  if (!userconfig) return 0;
  return userconfig->data_file_list;
}

/* Acquire "input" and "resources".
 */
 
int ps_userconfig_commit_paths(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  const char *path;

  /* Input config file doesn't have to exist, but its parent directory does.
   */
  if (path=ps_file_list_get_first_existing_file(userconfig->input_file_list)) {
    if (ps_userconfig_set_field_as_string(userconfig,ps_userconfig_search_field(userconfig,"input",5),path,-1)<0) return -1;
  } else if (path=ps_file_list_get_first_existing_parent(userconfig->input_file_list)) {
    if (ps_userconfig_set_field_as_string(userconfig,ps_userconfig_search_field(userconfig,"input",5),path,-1)<0) return -1;
  } else {
    int choicec=ps_file_list_count(userconfig->input_file_list);
    ps_log(CONFIG,ERROR,"Failed to acquire input config path from %d choice%s.",choicec,(choicec==1)?"":"s");
    return -1;
  }

  /* Data must exist, and can be either regular or directory.
   */
  if (!(path=ps_file_list_get_first_existing_file_or_directory(userconfig->data_file_list))) {
    int choicec=ps_file_list_count(userconfig->data_file_list);
    ps_log(CONFIG,ERROR,"Failed to acquire data path from %d choice%s.",choicec,(choicec==1)?"":"s");
    return -1;
  }
  if (ps_userconfig_set_field_as_string(userconfig,ps_userconfig_search_field(userconfig,"resources",9),path,-1)<0) return -1;
  
  return 0;
}

/* Set path to some existing file.
 */

int ps_userconfig_set_first_existing_path(struct ps_userconfig *userconfig,const char *path,...) {
  if (!userconfig) return -1;
  va_list vargs;
  va_start(vargs,path);
  while (path) {
    if (path[0]) { // Skip empty strings.
      struct stat st;
      if (stat(path,&st)>=0) {
        if (S_ISREG(st.st_mode)) {
          return ps_userconfig_set_path(userconfig,path);
        }
      }
    }
    path=va_arg(vargs,const char*);
  }
  return -1;
}

/* Set path directly.
 */

int ps_userconfig_set_path(struct ps_userconfig *userconfig,const char *path) {
  if (!userconfig) return -1;
  if (userconfig->path) free(userconfig->path);
  if (path) {
    if (!(userconfig->path=strdup(path))) return -1;
  } else {
    userconfig->path=0;
  }
  return 0;
}

const char *ps_userconfig_get_path(const struct ps_userconfig *userconfig) {
  if (!userconfig) return 0;
  return userconfig->path;
}

/* Acquire path from file list.
 */
 
int ps_userconfig_acquire_path(struct ps_userconfig *userconfig,int reset) {
  if (!userconfig) return -1;
  if (userconfig->path&&!reset) return 0;

  const char *path=ps_file_list_get_first_existing_file(userconfig->config_file_list);
  if (path) return ps_userconfig_set_path(userconfig,path);

  path=ps_file_list_get_first_existing_parent(userconfig->config_file_list);
  if (path) return ps_userconfig_set_path(userconfig,path);

  int choicec=ps_file_list_count(userconfig->config_file_list);
  ps_log(CONFIG,ERROR,"Failed to acquire valid configuration path from %d choice%s.",choicec,(choicec==1)?"":"s");
  return -1;
}

/* Encode text.
 */

static int ps_userconfig_encode_value_to_buffer(struct ps_buffer *output,const struct ps_userconfig *userconfig,int fldp) {
  while (1) {
    int err=ps_userconfig_get_field_as_string(output->v+output->c,output->a-output->c,userconfig,fldp);
    if (err<0) return -1;
    if (output->c>=INT_MAX-err) return -1;
    if (output->c<=output->a-err) {
      output->c+=err;
      break;
    }
    if (ps_buffer_require(output,err)<0) return -1;
  }
  return 0;
}
 
int ps_userconfig_encode(struct ps_buffer *output,const struct ps_userconfig *userconfig) {
  const struct ps_userconfig_field *field=userconfig->fldv;
  int fldp=0; for (;fldp<userconfig->fldc;fldp++,field++) {
    if (ps_buffer_append(output,field->k,-1)<0) return -1;
    if (ps_buffer_append(output,"=",1)<0) return -1;
    if (ps_userconfig_encode_value_to_buffer(output,userconfig,fldp)<0) return -1;
    if (ps_buffer_append(output,"\n",1)<0) return -1;
  }
  return 0;
}

/* Decode text.
 */

static int ps_userconfig_load_line(struct ps_userconfig *userconfig,const char *src,int srcc,int lineno) {

  /* Skip leading whitespace. Abort successfully if whitespace only. */
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;

  /* Key runs to the first whitespace, equal, or colon. */
  const char *k=src+srcp;
  int kc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!=':')&&(src[srcp]!='=')) { srcp++; kc++; }

  /* Consume any amount of whitespace, plus one of equal or colon. */
  int have_separator=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if ((src[srcp]=='=')||(src[srcp]==':')) {
      if (have_separator) break;
      have_separator=1;
      srcp++;
      continue;
    }
    break;
  }

  /* Value runs from here to end of line. Trim trailing whitespace. */
  const char *v=src+srcp;
  int vc=srcc-srcp;
  while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;

  /* Now set it as text, just like from any other source. */
  if (ps_userconfig_set(userconfig,k,kc,v,vc)<0) {
    ps_log(CONFIG,ERROR,"%s:%d: Failed to set '%.*s' to '%.*s'.",userconfig->path,lineno,kc,k,vc,v);
    return -1;
  }
  return 0;
}

int ps_userconfig_decode(struct ps_userconfig *userconfig,const char *src,int srcc) {
  if (!userconfig) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (ps_line_reader_next(&reader)>0) {
    if (ps_userconfig_load_line(userconfig,reader.line,reader.linec,reader.lineno)<0) return -1;
  }
  return 0;
}

/* Context for reencode file.
 */

#define PS_USERCONFIG_LINE_PRESENCE_ABSENT    0 /* Haven't seen it yet. */
#define PS_USERCONFIG_LINE_PRESENCE_DONE      1 /* Already committed to output. */
#define PS_USERCONFIG_LINE_PRESENCE_DIFFERENT 2 /* Found once previously, uncommented, with different value. */
#define PS_USERCONFIG_LINE_PRESENCE_COMMENTED 3 /* Found at least once previously, commented, with different value. */

struct ps_userconfig_line {
  int presence;
  int srcp; // Invalid if presence==ABSENT
  int dstp; // Corrollary of (srcp), in output
};

struct ps_userconfig_reencode_context {
  const struct ps_userconfig *userconfig;
  const char *src; // We don't use a line reader because we have to preserve newlines verbatim.
  int srcc;
  int srcp;
  struct ps_buffer *dst;
  struct ps_userconfig_line *linev;
};

static int ps_userconfig_context_setup(struct ps_userconfig_reencode_context *ctx) {
  if (!(ctx->linev=calloc(sizeof(struct ps_userconfig_line),ctx->userconfig->fldc))) return -1;
  return 0;
}

static void ps_userconfig_context_cleanup(struct ps_userconfig_reencode_context *ctx) {
  if (ctx->linev) free(ctx->linev);
}

/* Adjust all output positions beyond (p).
 */

static int ps_userconfig_reencode_adjusted_output(struct ps_userconfig_reencode_context *ctx,int p,int d) {
  struct ps_userconfig_line *line=ctx->linev;
  int i=ctx->userconfig->fldc;
  for (;i-->0;line++) {
    if (line->dstp>p) line->dstp+=d;
  }
  return 0;
}

/* Reencode: Compare value text to stored value.
 * Return 0==different or 1==equivalent, nothing else.
 */

static int ps_userconfig_reencode_check_value(struct ps_userconfig_reencode_context *ctx,int fldp,const char *nv,int nvc) {
  const struct ps_userconfig_field *field=ctx->userconfig->fldv+fldp;
  switch (field->type) {
  
    case PS_USERCONFIG_TYPE_BOOLEAN: {
        int v=ps_bool_eval(nv,nvc);
        return (v==field->b.v)?1:0;
      } break;

    case PS_USERCONFIG_TYPE_INTEGER: {
        int v;
        if (ps_int_eval(&v,nv,nvc)<0) return 0;
        return (v==field->i.v)?1:0;
      } break;

    case PS_USERCONFIG_TYPE_STRING: {
        if (nvc!=field->s.c) return 0;
        if (memcmp(nv,field->s.v,nvc)) return 0;
      } return 1;

    case PS_USERCONFIG_TYPE_PATH: {
        if (nvc!=field->p.c) return 0;
        if (memcmp(nv,field->p.v,nvc)) return 0;
      } return 1;

  }
  return 0;
}

/* Reencode: Process single line of input.
 */

static int ps_userconfig_reencode_line(struct ps_userconfig_reencode_context *ctx,const char *line,int linec,int line_srcp) {
  int linep=0;

  /* Skip leading whitespace. If that's all there is, preserve it verbatim. */
  while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
  if (linep>=linec) {
    return ps_buffer_append(ctx->dst,line,linec);
  }

  /* If it's commented with a single '#', note that, skip it, and continue processing. */
  int comment=0;
  if (line[linep]=='#') {
    comment=1;
    linep++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
  }
  int startp=linep; // Starting position if commented.

  /* Now locate a trailing comment and identify the end of the useful part of the line. */
  int cmtp=linec;
  int i=linep;
  for (;i<linec;i++) if (line[i]=='#') {
    cmtp=i;
    break;
  }
  while ((cmtp>linep)&&((unsigned char)line[cmtp-1]<=0x20)) cmtp--;

  /* Split key and value.
   * We must follow the same somewhat goofy rules as ps_userconfig_load_line().
   */
  const char *k=line+linep;
  int kc=0,have_separator=0;
  while ((linep<cmtp)&&((unsigned char)line[linep]>0x20)&&(line[linep]!='=')&&(line[linep]!=':')) { linep++; kc++; }
  while (linep<cmtp) {
    if ((line[linep]==':')||(line[linep]=='=')) {
      if (have_separator) break;
      have_separator=1;
    } else if ((unsigned char)line[linep]>0x20) {
      break;
    }
    linep++;
  }
  const char *v=line+linep;
  int vc=cmtp-linep;

  /* Look up key. */
  int fldp=ps_userconfig_search_field(ctx->userconfig,k,kc);

  /* If the key is not known, either leave comment untouched or prefix the line with '#'. */
  if (fldp<0) {
    if (!comment) {
      if (ps_buffer_append(ctx->dst,"# ",2)<0) return -1;
    }
    if (ps_buffer_append(ctx->dst,line,linec)<0) return -1;
    return 0;
  }

  /* If we've already committed this field, comment out this line. Do not change line presence. */
  int presence=ctx->linev[fldp].presence;
  if (presence==PS_USERCONFIG_LINE_PRESENCE_DONE) {
    if (!comment) {
      if (ps_buffer_append(ctx->dst,"# ",2)<0) return -1;
    }
    if (ps_buffer_append(ctx->dst,line,linec)<0) return -1;
    return 0;
  }

  /* Parse value and check whether it is equivalent to the value we have. */
  int equivalent=ps_userconfig_reencode_check_value(ctx,fldp,v,vc);

  /* If the values are equivalent, we're keeping it. Uncomment if necessary. */
  if (equivalent) {
    int ndstp=ctx->dst->c;
    if (comment) {
      if (ps_buffer_append(ctx->dst,line+startp,linec-startp)<0) return -1;
    } else {
      if (ps_buffer_append(ctx->dst,line,linec)<0) return -1;
    }
    if (presence==PS_USERCONFIG_LINE_PRESENCE_DIFFERENT) { // Comment out prior occurrence.
      if (ps_buffer_replace(ctx->dst,ctx->linev[fldp].dstp,0,"# ",2)<0) return -1;
      if (ps_userconfig_reencode_adjusted_output(ctx,ctx->linev[fldp].dstp,2)<0) return -1;
    }
    ctx->linev[fldp].presence=PS_USERCONFIG_LINE_PRESENCE_DONE;
    ctx->linev[fldp].srcp=line_srcp;
    ctx->linev[fldp].dstp=ndstp;
    return 0;
  }

  /* If we already have a DIFFERENT value, keep it and ensure that this line be commented. */
  if (presence==PS_USERCONFIG_LINE_PRESENCE_DIFFERENT) {
    if (!comment) {
      if (ps_buffer_append(ctx->dst,"# ",2)<0) return -1;
    }
    if (ps_buffer_append(ctx->dst,line,linec)<0) return -1;
    return 0;
  }

  /* At this point, we can update the line presence to either DIFFERENT or COMMENTED. */
  ctx->linev[fldp].dstp=ctx->dst->c;
  ctx->linev[fldp].srcp=line_srcp;
  ctx->linev[fldp].presence=(comment?PS_USERCONFIG_LINE_PRESENCE_COMMENTED:PS_USERCONFIG_LINE_PRESENCE_DIFFERENT);
  if (ps_buffer_append(ctx->dst,line,linec)<0) return -1;

  return 0;
}

/* Reencode: replace value in existing line.
 */

static int ps_userconfig_reencode_populate_different_line(struct ps_userconfig_reencode_context *ctx,int fldp) {

  /* Locate value in output. */
  const char *src=ctx->dst->v+ctx->linev[fldp].dstp;
  int srcc=ctx->dst->c-ctx->linev[fldp].dstp;
  int srcp=0,sep=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!=':')&&(src[srcp]!='=')&&(src[srcp]!='#')) srcp++;
  while (srcp<srcc) {
    if ((src[srcp]==':')||(src[srcp]=='=')) {
      if (sep) break;
      sep=1;
    } else if ((unsigned char)src[srcp]>0x20) {
      break;
    } else if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      break;
    }
    srcp++;
  }
  int oldvp=srcp;
  int oldvc=0;
  while ((srcp<srcc)&&(src[srcp]!='#')&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) { srcp++; oldvc++; }
  while (oldvc&&((unsigned char)src[oldvp+oldvc-1]<=0x20)) oldvc--;
  oldvp+=ctx->linev[fldp].dstp;

  /* Encode new value in a separate buffer. */
  struct ps_buffer tmp={0};
  if (ps_userconfig_encode_value_to_buffer(&tmp,ctx->userconfig,fldp)<0) {
    ps_buffer_cleanup(&tmp);
    return -1;
  }

  /* Replace. */
  int d=tmp.c-oldvc;
  if (ps_buffer_replace(ctx->dst,oldvp,oldvc,tmp.v,tmp.c)<0) {
    ps_buffer_cleanup(&tmp);
    return -1;
  }
  ps_buffer_cleanup(&tmp);
  if (ps_userconfig_reencode_adjusted_output(ctx,oldvp,d)<0) return -1;
  ctx->linev[fldp].presence=PS_USERCONFIG_LINE_PRESENCE_DONE;

  return 0;
}

/* Reencode: insert line after a commented one.
 */

static int ps_userconfig_reencode_insert_after_line(struct ps_userconfig_reencode_context *ctx,int fldp) {

  struct ps_buffer tmp={0};
  if (
    (ps_buffer_append(&tmp,ctx->userconfig->fldv[fldp].k,ctx->userconfig->fldv[fldp].kc)<0)||
    (ps_buffer_append(&tmp,"=",1)<0)||
    (ps_userconfig_encode_value_to_buffer(&tmp,ctx->userconfig,fldp)<0)||
    (ps_buffer_append(&tmp,"\n",1)<0)
  ) {
    ps_buffer_cleanup(&tmp);
    return -1;
  }

  int dstp=ctx->linev[fldp].dstp;
  while (dstp<ctx->dst->c) {
    char ch=ctx->dst->v[dstp];
    if (++dstp>=ctx->dst->c) break;
    if ((ch==0x0a)||(ch==0x0d)) {
      if ((ctx->dst->v[dstp]!=ch)&&((ctx->dst->v[dstp]==0x0a)||(ctx->dst->v[dstp]==0x0d))) dstp++;
      break;
    }
  }

  if (ps_buffer_replace(ctx->dst,dstp,0,tmp.v,tmp.c)<0) {
    ps_buffer_cleanup(&tmp);
    return -1;
  }
  if (ps_userconfig_reencode_adjusted_output(ctx,dstp-1,tmp.c)<0) return -1;
  ps_buffer_cleanup(&tmp);
  
  ctx->linev[fldp].dstp=dstp;
  ctx->linev[fldp].presence=PS_USERCONFIG_LINE_PRESENCE_DONE;
  return 0;
}

/* Reencode file with established context.
 */

static int ps_userconfig_reencode_inner(struct ps_userconfig_reencode_context *ctx) {

  /* First read through the input, copying to output.
   */
  while (ctx->srcp<ctx->srcc) {

    /* Measure line, including terminator (LF, CR, LFCR, CRLF). */
    int line_srcp=ctx->srcp;
    const char *line=ctx->src+ctx->srcp;
    int linec=0;
    while (ctx->srcp<ctx->srcc) {
      if ((line[linec]==0x0a)||(line[linec]==0x0d)) {
        linec++;
        ctx->srcp++;
        if ((ctx->srcp<ctx->srcc)&&(line[linec]!=line[linec-1])&&((line[linec]==0x0a)||(line[linec]==0x0d))) {
          linec++;
          ctx->srcp++;
        }
        break;
      }
      linec++;
      ctx->srcp++;
    }

    if (ps_userconfig_reencode_line(ctx,line,linec,line_srcp)<0) return -1;
    
  }

  /* Apply deferred changes. */
  struct ps_userconfig_line *line=ctx->linev;
  const struct ps_userconfig_field *field=ctx->userconfig->fldv;
  int fldp=0; 
  for (;fldp<ctx->userconfig->fldc;fldp++,line++,field++) {
    if (line->presence==PS_USERCONFIG_LINE_PRESENCE_DONE) continue;
    switch (line->presence) {
    
      case PS_USERCONFIG_LINE_PRESENCE_ABSENT: {
          line->presence=PS_USERCONFIG_LINE_PRESENCE_DONE;
          line->srcp=-1;
          line->dstp=ctx->dst->c;
          if (ps_buffer_append(ctx->dst,field->k,field->kc)<0) return -1;
          if (ps_buffer_append(ctx->dst,"=",1)<0) return -1;
          if (ps_userconfig_encode_value_to_buffer(ctx->dst,ctx->userconfig,fldp)<0) return -1;
          if (ps_buffer_append(ctx->dst,"\n",1)<0) return -1;
        } break;

      case PS_USERCONFIG_LINE_PRESENCE_DIFFERENT: {
          if (ps_userconfig_reencode_populate_different_line(ctx,fldp)<0) return -1;
        } break;

      case PS_USERCONFIG_LINE_PRESENCE_COMMENTED: {
          if (ps_userconfig_reencode_insert_after_line(ctx,fldp)<0) return -1;
        } break;
    }
  }
  
  return 0;
}

/* Reencode file, main entry point.
 */
 
int ps_userconfig_reencode(struct ps_buffer *dst,const char *src,int srcc,const struct ps_userconfig *userconfig) {
  if (!dst||!userconfig) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc0=dst->c;
  struct ps_userconfig_reencode_context ctx={
    .userconfig=userconfig,
    .src=src,
    .srcc=srcc,
    .dst=dst,
  };
  if (ps_userconfig_context_setup(&ctx)<0) {
    ps_userconfig_context_cleanup(&ctx);
    return -1;
  }
  if (ps_userconfig_reencode_inner(&ctx)<0) {
    ps_userconfig_context_cleanup(&ctx);
    dst->c=dstc0;
    return -1;
  }
  ps_userconfig_context_cleanup(&ctx);
  return 0;
}

/* Load file.
 */

int ps_userconfig_load_file(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  if (ps_userconfig_acquire_path(userconfig,0)<0) return -1;
  if (!userconfig->path) return 0;
  char *src=0;
  int srcc=ps_file_read(&src,userconfig->path);
  if (srcc<0) {
    ps_log(CONFIG,WARNING,"%s: Failed to read configuration from file.",userconfig->path);
    return 0;
  }
  int err=ps_userconfig_decode(userconfig,src,srcc);
  if (src) free(src);
  if (err<0) return -1;
  return 0;
}

/* Save file.
 */

int ps_userconfig_save_file(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  if (ps_userconfig_acquire_path(userconfig,0)<0) return -1;
  if (!userconfig->path) return -1;
  if (!userconfig->dirty) return 0;
  struct ps_buffer buffer={0};

  /* If it already exists, use 'reencode'. */
  char *oldsrc=0;
  int oldsrcc=ps_file_read(&oldsrc,userconfig->path);
  if ((oldsrcc>=0)&&oldsrc) {
    if (ps_userconfig_reencode(&buffer,oldsrc,oldsrcc,userconfig)<0) {
      free(oldsrc);
      ps_buffer_cleanup(&buffer);
      return -1;
    }

  /* File doesn't exist, write it from scratch. */
  } else {
    if (ps_userconfig_encode(&buffer,userconfig)<0) {
      ps_buffer_cleanup(&buffer);
      return -1;
    }
  }

  /* Write it. */
  if (ps_mkdir_parents(userconfig->path)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  int err=ps_file_write(userconfig->path,buffer.v,buffer.c);
  ps_buffer_cleanup(&buffer);
  if (err<0) return -1;
  userconfig->dirty=0;
  ps_log(CONFIG,INFO,"%s: Rewrote general config file",userconfig->path);
  return 0;
}

/* Dirty flag.
 */
 
int ps_userconfig_set_dirty(struct ps_userconfig *userconfig,int dirty) {
  if (!userconfig) return -1;
  userconfig->dirty=dirty?1:0;
  return 0;
}

/* Load argv.
 */
 
int ps_userconfig_load_argv(struct ps_userconfig *userconfig,int argc,char **argv) {
  if (!userconfig) return -1;
  int processedc=0;
  int i=1; for (;i<argc;i++) {
    const char *arg=argv[i];
    if (!arg||!arg[0]) continue;
    
    if ((arg[0]!='-')||(arg[1]!='-')) {
      ps_log(CONFIG,ERROR,"Unexpected command-line argument '%s'.",arg);
      return -1;
    }

    const char *k=arg+2,*v=0;
    int kc=0,vc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    if (k[kc]) {
      v=k+kc+1;
      while (v[vc]) vc++;
    } else {
      if ((kc>=3)&&!memcmp(k,"no-",3)) {
        v="false";
        vc=5;
        k+=3;
        kc-=3;
      } else {
        v="true";
        vc=4;
      }
    }

    if (ps_userconfig_set(userconfig,k,kc,v,vc)<0) return -1;
    processedc++;
  }
  return processedc;
}

/* Read declarations.
 */

int ps_userconfig_count_fields(const struct ps_userconfig *userconfig) {
  if (!userconfig) return 0;
  return userconfig->fldc;
}

int ps_userconfig_search_field(const struct ps_userconfig *userconfig,const char *k,int kc) {
  if (!userconfig) return -1;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  int lo=0,hi=userconfig->fldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (kc<userconfig->fldv[ck].kc) hi=ck;
    else if (kc>userconfig->fldv[ck].kc) lo=ck+1;
    else {
      int cmp=memcmp(k,userconfig->fldv[ck].k,kc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return ck;
    }
  }
  return -lo-1;
}

int ps_userconfig_get_field_declaration(int *type,const char **k,const struct ps_userconfig *userconfig,int fldp) {
  if (!userconfig) return -1;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return -1;
  const struct ps_userconfig_field *field=userconfig->fldv+fldp;
  if (type) *type=field->type;
  if (k) *k=field->k;
  return field->kc;
}

/* Read field values.
 */

int ps_userconfig_get_field_as_int(const struct ps_userconfig *userconfig,int fldp) {
  if (!userconfig) return 0;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return 0;
  const struct ps_userconfig_field *field=userconfig->fldv+fldp;
  switch (field->type) {
    case PS_USERCONFIG_TYPE_BOOLEAN: return field->b.v;
    case PS_USERCONFIG_TYPE_INTEGER: return field->i.v;
  }
  return 0;
}

int ps_userconfig_get_field_as_string(char *dst,int dsta,const struct ps_userconfig *userconfig,int fldp) {
  if (!dst||(dsta<0)) dsta=0;
  if (!userconfig) return -1;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return -1;
  const struct ps_userconfig_field *field=userconfig->fldv+fldp;
  switch (field->type) {
    case PS_USERCONFIG_TYPE_BOOLEAN: return ps_decsint_repr(dst,dsta,field->b.v);
    case PS_USERCONFIG_TYPE_INTEGER: return ps_decsint_repr(dst,dsta,field->i.v);
    case PS_USERCONFIG_TYPE_STRING: return ps_strcpy(dst,dsta,field->s.v,field->s.c);
    case PS_USERCONFIG_TYPE_PATH: return ps_strcpy(dst,dsta,field->p.v,field->p.c);
  }
  return -1;
}

int ps_userconfig_peek_field_as_string(void *dstpp,const struct ps_userconfig *userconfig,int fldp) {
  if (!userconfig) return -1;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return -1;
  const struct ps_userconfig_field *field=userconfig->fldv+fldp;
  switch (field->type) {
    case PS_USERCONFIG_TYPE_STRING: {
        if (dstpp) *(void**)dstpp=field->s.v;
        return field->s.c;
      }
    case PS_USERCONFIG_TYPE_PATH: {
        if (dstpp) *(void**)dstpp=field->p.v;
        return field->p.c;
      }
  }
  return -1;
}

/* More convenient getters.
 */
 
int ps_userconfig_get_int(const struct ps_userconfig *userconfig,const char *k,int kc) {
  int fldp=ps_userconfig_search_field(userconfig,k,kc);
  if (fldp<0) return 0;
  return ps_userconfig_get_field_as_int(userconfig,fldp);
}

const char *ps_userconfig_get_str(const struct ps_userconfig *userconfig,const char *k,int kc) {
  int fldp=ps_userconfig_search_field(userconfig,k,kc);
  if (fldp<0) return 0;
  const char *v=0;
  if (ps_userconfig_peek_field_as_string(&v,userconfig,fldp)<0) return 0;
  return v;
}

/* Confirm that file exists.
 */

static int ps_userconfig_file_exists(const char *src,int srcc) {
  if (srcc<1) return 0;
  char path[1024];
  if (srcc>=sizeof(path)) return 0;
  memcpy(path,src,srcc);
  path[srcc]=0;
  struct stat st;
  if (stat(path,&st)<0) return 0;
  if (S_ISREG(st.st_mode)) return 1;
  return 0;
}

/* Set field with index and known type.
 */

int ps_userconfig_set_field_as_int(struct ps_userconfig *userconfig,int fldp,int src) {
  if (!userconfig) return -1;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  switch (field->type) {
  
    case PS_USERCONFIG_TYPE_BOOLEAN: {
        src=src?1:0;
        if (field->b.v==src) return 0;
        field->b.v=src;
        userconfig->dirty=1;
      } return 0;

    case PS_USERCONFIG_TYPE_INTEGER: {
        if (field->i.v==src) return 0;
        field->i.v=src;
        userconfig->dirty=1;
      } return 0;

  }
  return -1;
}

int ps_userconfig_set_field_as_string(struct ps_userconfig *userconfig,int fldp,const char *src,int srcc) {
  if (!userconfig) return -1;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  switch (field->type) {

    case PS_USERCONFIG_TYPE_BOOLEAN: {
        int nv=ps_bool_eval(src,srcc);
        if (nv<0) {
          ps_log(CONFIG,ERROR,"Value for '%.*s' must be boolean, '%.*s' is invalid",field->kc,field->k,srcc,src);
          return -1;
        }
        if (field->b.v==nv) return 0;
        userconfig->dirty=1;
        field->b.v=nv;
      } return 0;

    case PS_USERCONFIG_TYPE_INTEGER: {
        int nv;
        if (ps_int_eval(&nv,src,srcc)<0) {
          ps_log(CONFIG,ERROR,"Value for '%.*s' must be integer, '%.*s' is invalid",field->kc,field->k,srcc,src);
          return -1;
        }
        if ((nv<field->i.lo)||(nv>field->i.hi)) {
          ps_log(CONFIG,ERROR,"Value %d out of range for '%.*s' (%d..%d)",nv,field->kc,field->k,field->i.lo,field->i.hi);
          return -1;
        }
        if (field->i.v==nv) return 0;
        userconfig->dirty=1;
        field->i.v=nv;
      } return 0;

    case PS_USERCONFIG_TYPE_STRING: {
        if ((srcc==field->s.c)&&!memcmp(src,field->s.v,srcc)) return 0;
        char *nv=malloc(srcc+1);
        if (!nv) return -1;
        memcpy(nv,src,srcc);
        nv[srcc]=0;
        if (field->s.v) free(field->s.v);
        field->s.v=nv;
        field->s.c=srcc;
        userconfig->dirty=1;
      } return 0;

    case PS_USERCONFIG_TYPE_PATH: {
        if (field->p.must_exist) {
          if (!ps_userconfig_file_exists(src,srcc)) {
            ps_log(CONFIG,ERROR,"Value for '%.*s' must be an existing file, '%.*s' is invalid",field->kc,field->k,srcc,src);
            return -1;
          }
        }
        if ((srcc==field->p.c)&&!memcmp(src,field->p.v,srcc)) return 0;
        char *nv=malloc(srcc+1);
        if (!nv) return -1;
        memcpy(nv,src,srcc);
        nv[srcc]=0;
        if (field->p.v) free(field->p.v);
        field->p.v=nv;
        field->p.c=srcc;
        userconfig->dirty=1;
      } return 0;

  }
  return -1;
}

/* Set field, general entry point.
 */
  
int ps_userconfig_set(struct ps_userconfig *userconfig,const char *k,int kc,const char *v,int vc) {
  if (!userconfig) return -1;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }

  /* If the key is "input" or "resources", push the value in the front of the associated file list.
   */
  if ((kc==5)&&!memcmp(k,"input",5)) {
    return ps_file_list_add(userconfig->input_file_list,0,v,vc);
  }
  if ((kc==9)&&!memcmp(k,"resources",9)) {
    return ps_file_list_add(userconfig->data_file_list,0,v,vc);
  }

  /* Normal cases, set the field directly.
   */
  int fldp=ps_userconfig_search_field(userconfig,k,kc);
  if (fldp<0) {
    ps_log(CONFIG,ERROR,"Unknown config key '%.*s'",kc,k);
    return -1;
  }
  return ps_userconfig_set_field_as_string(userconfig,fldp,v,vc);
}

/* Add field.
 */

static int ps_userconfig_fldv_require(struct ps_userconfig *userconfig) {
  if (userconfig->fldc<userconfig->flda) return 0;
  int na=userconfig->flda+32;
  if (na>INT_MAX/sizeof(struct ps_userconfig_field)) return -1;
  void *nv=realloc(userconfig->fldv,sizeof(struct ps_userconfig_field)*na);
  if (!nv) return -1;
  userconfig->fldv=nv;
  userconfig->flda=na;
  return 0;
}

static int ps_userconfig_declare(struct ps_userconfig *userconfig,const char *k,int type) {
  if (!userconfig||!k) return -1;
  int kc=0; while (k[kc]) kc++;
  if (kc>255) return -1;
  int fldp=ps_userconfig_search_field(userconfig,k,kc);
  if (fldp>=0) {
    if (userconfig->fldv[fldp].type==type) return fldp;
    ps_log(CONFIG,ERROR,"Incompatible redefinition of config key '%.*s'",kc,k);
    return -1;
  }
  fldp=-fldp-1;
  if (ps_userconfig_fldv_require(userconfig)<0) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  char *nk=malloc(kc+1);
  if (!nk) return -1;
  memcpy(nk,k,kc);
  nk[kc]=0;
  memmove(field+1,field,sizeof(struct ps_userconfig_field)*(userconfig->fldc-fldp));
  userconfig->fldc++;
  memset(field,0,sizeof(struct ps_userconfig_field));
  field->k=nk;
  field->kc=kc;
  field->type=type;
  return fldp;
}

/* Remove field.
 */

static int ps_userconfig_remove_field(struct ps_userconfig *userconfig,int fldp) {
  if (!userconfig) return -1;
  if ((fldp<0)||(fldp>=userconfig->fldc)) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  ps_userconfig_field_cleanup(field);
  userconfig->fldc--;
  memmove(field,field+1,sizeof(struct ps_userconfig_field)*(userconfig->fldc-fldp));
  return 0;
}

/* Declare field, public entry points.
 */

int ps_userconfig_declare_boolean(struct ps_userconfig *userconfig,const char *k,int v) {
  int fldp=ps_userconfig_declare(userconfig,k,PS_USERCONFIG_TYPE_BOOLEAN);
  if (fldp<0) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  field->b.v=v?1:0;
  return fldp;
}

int ps_userconfig_declare_integer(struct ps_userconfig *userconfig,const char *k,int v,int lo,int hi) {
  if ((v<lo)||(v>hi)) {
    ps_log(CONFIG,ERROR,"Value %d not allowed for key '%s' (%d..%d)",v,k,lo,hi);
    return -1;
  }
  int fldp=ps_userconfig_declare(userconfig,k,PS_USERCONFIG_TYPE_INTEGER);
  if (fldp<0) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  field->i.lo=lo;
  field->i.hi=hi;
  field->i.v=v;
  return fldp;
}

int ps_userconfig_declare_string(struct ps_userconfig *userconfig,const char *k,const char *v,int vc) {
  int fldp=ps_userconfig_declare(userconfig,k,PS_USERCONFIG_TYPE_STRING);
  if (fldp<0) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  if (ps_userconfig_set_field_as_string(userconfig,fldp,v,vc)<0) {
    ps_userconfig_remove_field(userconfig,fldp);
    return -1;
  }
  return fldp;
}

int ps_userconfig_declare_path(struct ps_userconfig *userconfig,const char *k,const char *v,int vc,int must_exist) {
  int fldp=ps_userconfig_declare(userconfig,k,PS_USERCONFIG_TYPE_PATH);
  if (fldp<0) return -1;
  struct ps_userconfig_field *field=userconfig->fldv+fldp;
  field->p.must_exist=must_exist;
  if (ps_userconfig_set_field_as_string(userconfig,fldp,v,vc)<0) {
    ps_userconfig_remove_field(userconfig,fldp);
    return -1;
  }
  return fldp;
}

/* Undeclare field.
 */
 
int ps_userconfig_undeclare(struct ps_userconfig *userconfig,const char *k) {
  if (!userconfig) return -1;
  int fldp=ps_userconfig_search_field(userconfig,k,-1);
  if (fldp<0) return -1;
  if (ps_userconfig_remove_field(userconfig,fldp)<0) return -1;
  return 0;
}
