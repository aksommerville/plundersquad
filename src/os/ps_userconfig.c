#include "ps.h"
#include "os/ps_ioc.h"
#include "os/ps_userconfig.h"
#include "os/ps_fs.h"
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

  return userconfig;
}

void ps_userconfig_del(struct ps_userconfig *userconfig) {
  if (!userconfig) return;
  if (userconfig->refc-->1) return;

  if (userconfig->path) free(userconfig->path);

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

/* Set path.
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

/* Encode text.
 */
 
int ps_userconfig_encode(struct ps_buffer *output,const struct ps_userconfig *userconfig) {
  const struct ps_userconfig_field *field=userconfig->fldv;
  int fldp=0; for (;fldp<userconfig->fldc;fldp++,field++) {
    if (ps_buffer_append(output,field->k,-1)<0) return -1;
    if (ps_buffer_append(output,"=",1)<0) return -1;
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

static int ps_userconfig_load_text(struct ps_userconfig *userconfig,const char *src,int srcc) {
  struct ps_line_reader reader={0};
  if (ps_line_reader_begin(&reader,src,srcc,1)<0) return -1;
  while (ps_line_reader_next(&reader)>0) {
    if (ps_userconfig_load_line(userconfig,reader.line,reader.linec,reader.lineno)<0) return -1;
  }
  return 0;
}

/* Load file.
 */

int ps_userconfig_load_file(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  if (!userconfig->path) return 0;
  char *src=0;
  int srcc=ps_file_read(&src,userconfig->path);
  if (srcc<0) {
    ps_log(CONFIG,WARNING,"%s: Failed to read configuration from file.",userconfig->path);
    return 0;
  }
  int err=ps_userconfig_load_text(userconfig,src,srcc);
  if (src) free(src);
  if (err<0) return -1;
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

/* Save file.
 */

int ps_userconfig_save_file(struct ps_userconfig *userconfig) {
  if (!userconfig) return -1;
  if (!userconfig->path) return -1;
  struct ps_buffer buffer={0};
  if (ps_userconfig_encode(&buffer,userconfig)<0) {
    ps_buffer_cleanup(&buffer);
    return -1;
  }
  if (ps_mkdir_parents(userconfig->path)<0) return -1;
  int err=ps_file_write(userconfig->path,buffer.v,buffer.c);
  ps_buffer_cleanup(&buffer);
  return err;
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
    case PS_USERCONFIG_TYPE_BOOLEAN: field->b.v=src?1:0; return 0;
    case PS_USERCONFIG_TYPE_INTEGER: field->i.v=src; return 0;
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
        field->i.v=nv;
      } return 0;

    case PS_USERCONFIG_TYPE_STRING: {
        char *nv=malloc(srcc+1);
        if (!nv) return -1;
        memcpy(nv,src,srcc);
        nv[srcc]=0;
        if (field->s.v) free(field->s.v);
        field->s.v=nv;
        field->s.c=srcc;
      } return 0;

    case PS_USERCONFIG_TYPE_PATH: {
        if (field->p.must_exist) {
          if (!ps_userconfig_file_exists(src,srcc)) {
            ps_log(CONFIG,ERROR,"Value for '%.*s' must be an existing file, '%.*s' is invalid",field->kc,field->k,srcc,src);
            return -1;
          }
        }
        char *nv=malloc(srcc+1);
        if (!nv) return -1;
        memcpy(nv,src,srcc);
        nv[srcc]=0;
        if (field->p.v) free(field->p.v);
        field->p.v=nv;
        field->p.c=srcc;
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
