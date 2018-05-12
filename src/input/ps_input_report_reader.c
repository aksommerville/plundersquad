#include "ps.h"
#include "ps_input_report_reader.h"

/* Longest allowable report, in bits.
 * Anything in this ballpark must be a mistake.
 */
#define PS_INPUT_REPORT_SANITY_LIMIT 65536

/* Accessor types.
 */
#define PS_INPUT_REPORT_ACCESSOR_GENERIC     0 /* Unoptimized access. */
#define PS_INPUT_REPORT_ACCESSOR_U8          1
#define PS_INPUT_REPORT_ACCESSOR_S8          2
#define PS_INPUT_REPORT_ACCESSOR_U16LE       3
#define PS_INPUT_REPORT_ACCESSOR_U16BE       4
#define PS_INPUT_REPORT_ACCESSOR_S16LE       5
#define PS_INPUT_REPORT_ACCESSOR_S16BE       6
#define PS_INPUT_REPORT_ACCESSOR_S32LE       7
#define PS_INPUT_REPORT_ACCESSOR_S32BE       8
#define PS_INPUT_REPORT_ACCESSOR_BITS        9 /* Single byte containing up to eight one-bit fields. */

/* Object definition.
 */

struct ps_input_report_field {
  int p,c; // Position and size in bits.
  int sign;
  int aligned; // Hint that this field can be read as bytes.
  int fldid;
};

struct ps_input_report_accessor {
  int type;
  int fldix; // For single-field types only.
  union {
    struct ps_input_report_field generic; // PS_INPUT_REPORT_ACCESSOR_GENERIC
    struct { // PS_INPUT_REPORT_ACCESSOR_[US]{16,32}{BE,LE}
      int p; // Position in bytes (not bits, like ps_input_report_field).
      int fldid;
    } aligned;
    struct { // PS_INPUT_REPORT_ACCESSOR_BITS
      int p; // Byte position.
      int fldidv[8]; // [0]=0x01, ..., [7]=0x80
      int fldixv[8];
    } bits;
  };
};

struct ps_input_report_reader {
  int refc;
  int ready; // Nonzero after finalizing schema.
  char order; // '<' or '>'
  struct ps_input_report_field *fieldv;
  int fieldc,fielda;
  int insp; // Insertion point during schema construction, in bits.
  uint8_t *report; // Most recent report, only present when 'ready'.
  int reportc;
  uint8_t *buf; // Same size as (report), for transient comparison.
  struct ps_input_report_accessor *accessorv;
  int accessorc,accessora;
};

/* Object lifecycle.
 */
 
struct ps_input_report_reader *ps_input_report_reader_new() {
  struct ps_input_report_reader *reader=calloc(1,sizeof(struct ps_input_report_reader));
  if (!reader) return 0;

  reader->refc=1;
  reader->ready=0;
  reader->order='<';

  return reader;
}

void ps_input_report_reader_del(struct ps_input_report_reader *reader) {
  if (!reader) return;
  if (reader->refc-->1) return;

  if (reader->fieldv) free(reader->fieldv);
  if (reader->report) free(reader->report);
  if (reader->buf) free(reader->buf);
  if (reader->accessorv) free(reader->accessorv);

  free(reader);
}

int ps_input_report_reader_ref(struct ps_input_report_reader *reader) {
  if (!reader) return -1;
  if (reader->refc<1) return -1;
  if (reader->refc==INT_MAX) return -1;
  reader->refc++;
  return 0;
}

/* Header setup.
 */
 
int ps_input_report_reader_set_byte_order(struct ps_input_report_reader *reader,char order) {
  if (!reader) return -1;
  if (reader->ready) return -1;
  if ((order!='<')&&(order!='>')) return -1;
  reader->order=order;
  return 0;
}

/* Add padding.
 */

int ps_input_report_reader_add_padding(struct ps_input_report_reader *reader,int bitc) {
  if (!reader) return -1;
  if (reader->ready) return -1;
  if (bitc<0) return -1;
  if (reader->insp>INT_MAX-bitc) return -1;
  reader->insp+=bitc;
  return 0;
}

int ps_input_report_reader_pad_to_multiple(struct ps_input_report_reader *reader,int multiple) {
  if (!reader) return -1;
  if (multiple<1) return -1;
  if (multiple>1024) return -1; // Sanity check; we don't expect more than 32.
  int mod=reader->insp%multiple;
  if (!mod) return 0;
  int pad=multiple-mod;
  return ps_input_report_reader_add_padding(reader,pad);
}

/* Grow field list.
 */

static int ps_input_report_reader_require_field(struct ps_input_report_reader *reader) {
  if (reader->fieldc<reader->fielda) return 0;
  int na=reader->fielda+16;
  if (na>INT_MAX/sizeof(struct ps_input_report_field)) return -1;
  void *nv=realloc(reader->fieldv,sizeof(struct ps_input_report_field)*na);
  if (!nv) return -1;
  reader->fieldv=nv;
  reader->fielda=na;
  return 0;
}

/* Add field.
 */

int ps_input_report_reader_add_field(
  struct ps_input_report_reader *reader,
  int size,int sign,
  int fldid
) {
  if (!reader) return -1;
  if (reader->ready) return -1;
  if (size<1) return -1;
  if (size>32) return -1;
  if (reader->insp>PS_INPUT_REPORT_SANITY_LIMIT-size) return -1;
  
  if (ps_input_report_reader_require_field(reader)<0) return -1;
  struct ps_input_report_field *field=reader->fieldv+reader->fieldc;
  memset(field,0,sizeof(struct ps_input_report_field));
  field->p=reader->insp;
  field->c=size;
  field->sign=sign?1:0;
  field->fldid=fldid;
  if (!(field->p&7)&&!(field->c&7)) field->aligned=1;

  reader->insp+=size;
  int fldix=reader->fieldc++;
  return fldix;
}

/* Add field with explicit position.
 */
 
int ps_input_report_reader_add_field_at(
  struct ps_input_report_reader *reader,
  int position,int size,int sign,
  int fldid
) {
  if (!reader) return -1;
  if (reader->ready) return -1;
  if (size<1) return -1;
  if (size>32) return -1;
  if (position<0) return -1;
  if (position>PS_INPUT_REPORT_SANITY_LIMIT-size) return -1;

  if (ps_input_report_reader_require_field(reader)<0) return -1;
  struct ps_input_report_field *field=reader->fieldv+reader->fieldc;
  memset(field,0,sizeof(struct ps_input_report_field));
  field->p=position;
  field->c=size;
  field->sign=sign?1:0;
  field->fldid=fldid;
  if (!(field->p&7)&&!(field->c&7)) field->aligned=1;

  int np=position+size;
  if (np>reader->insp) reader->insp=np;
  int fldix=reader->fieldc++;
  return fldix;
}

/* Add accessor.
 */

static struct ps_input_report_accessor *ps_input_report_reader_add_accessor(struct ps_input_report_reader *reader,int type) {

  if (reader->accessorc>=reader->accessora) {
    int na=reader->accessora+8;
    if (na>INT_MAX/sizeof(struct ps_input_report_accessor)) return 0;
    void *nv=realloc(reader->accessorv,sizeof(struct ps_input_report_accessor)*na);
    if (!nv) return 0;
    reader->accessorv=nv;
    reader->accessora=na;
  }

  struct ps_input_report_accessor *accessor=reader->accessorv+reader->accessorc++;
  memset(accessor,0,sizeof(struct ps_input_report_accessor));
  accessor->type=type;
  accessor->fldix=-1;

  return accessor;
}

/* Get existing BITS accessor, or create one.
 */

static struct ps_input_report_accessor *ps_input_report_reader_get_accessor_bit(struct ps_input_report_reader *reader,int p) {
  struct ps_input_report_accessor *accessor=reader->accessorv;
  int i=reader->accessorc;
  for (;i-->0;accessor++) {
    if (accessor->type!=PS_INPUT_REPORT_ACCESSOR_BITS) continue;
    if (accessor->bits.p!=p) continue;
    return accessor;
  }
  if (!(accessor=ps_input_report_reader_add_accessor(reader,PS_INPUT_REPORT_ACCESSOR_BITS))) return 0;
  accessor->bits.p=p;
  return accessor;
}

/* Add a GENERIC accessor.
 */

static struct ps_input_report_accessor *ps_input_report_reader_add_accessor_generic(
  struct ps_input_report_reader *reader,const struct ps_input_report_field *field
) {
  struct ps_input_report_accessor *accessor=ps_input_report_reader_add_accessor(reader,PS_INPUT_REPORT_ACCESSOR_GENERIC);
  if (!accessor) return 0;
  memcpy(&accessor->generic,field,sizeof(struct ps_input_report_field));
  return accessor;
}

/* Rebuild accessors from fields.
 */

static int ps_input_report_reader_rebuild_accessors(struct ps_input_report_reader *reader) {
  reader->accessorc=0;
  const struct ps_input_report_field *field=reader->fieldv;
  int fldix=0;
  for (;fldix<reader->fieldc;fldix++,field++) {

    /* Is it a single word at 8, 16, or 32 bits? */
    if (field->aligned) switch (field->c) {
      case 8: {
          int type=(field->sign?PS_INPUT_REPORT_ACCESSOR_S8:PS_INPUT_REPORT_ACCESSOR_U8);
          struct ps_input_report_accessor *accessor=ps_input_report_reader_add_accessor(reader,type);
          if (!accessor) return -1;
          accessor->fldix=fldix;
          accessor->aligned.p=field->p>>3;
          accessor->aligned.fldid=field->fldid;
        } continue;
      case 16: {
          int type;
          if (reader->order=='>') {
            type=(field->sign?PS_INPUT_REPORT_ACCESSOR_S16BE:PS_INPUT_REPORT_ACCESSOR_U16BE);
          } else {
            type=(field->sign?PS_INPUT_REPORT_ACCESSOR_S16LE:PS_INPUT_REPORT_ACCESSOR_U16LE);
          }
          struct ps_input_report_accessor *accessor=ps_input_report_reader_add_accessor(reader,type);
          if (!accessor) return -1;
          accessor->fldix=fldix;
          accessor->aligned.p=field->p>>3;
          accessor->aligned.fldid=field->fldid;
        } continue;
      case 32: {
          // We emit values as signed 32-bit, so we just ignore the sign declaration here.
          int type;
          if (reader->order=='>') {
            type=PS_INPUT_REPORT_ACCESSOR_S32BE;
          } else {
            type=PS_INPUT_REPORT_ACCESSOR_S32LE;
          }
          struct ps_input_report_accessor *accessor=ps_input_report_reader_add_accessor(reader,type);
          if (!accessor) return -1;
          accessor->fldix=fldix;
          accessor->aligned.p=field->p>>3;
          accessor->aligned.fldid=field->fldid;
        } continue;
    }

    /* Is it a single bit? */
    if (field->c==1) {
      int p=field->p>>3;
      int bitp=(field->p&7);
      if (reader->order=='>') bitp=7-bitp;
      struct ps_input_report_accessor *accessor=ps_input_report_reader_get_accessor_bit(reader,p);
      if (!accessor) return -1;
      accessor->bits.fldidv[bitp]=field->fldid;
      accessor->bits.fldixv[bitp]=fldix;
      continue;
    }

    /* Something unusual. Read it generically. */
    struct ps_input_report_accessor *accessor=ps_input_report_reader_add_accessor_generic(reader,field);
    if (!accessor) return -1;
    accessor->fldix=fldix;
    
  }
  return 0;
}

/* Finish setup.
 */
 
int ps_input_report_reader_finish_setup(struct ps_input_report_reader *reader) {
  if (!reader) return -1;
  if (reader->ready) return -1;

  reader->reportc=(reader->insp+7)>>3;
  if (!(reader->report=calloc(1,reader->reportc))) {
    reader->reportc=0;
    return -1;
  }
  if (!(reader->buf=calloc(1,reader->reportc))) {
    free(reader->report);
    reader->report=0;
    reader->reportc=0;
    return -1;
  }

  if (ps_input_report_reader_rebuild_accessors(reader)<0) {
    free(reader->report);
    reader->report=0;
    reader->reportc=0;
    free(reader->buf);
    reader->buf=0;
    return -1;
  }

  reader->ready=1;
  return 0;
}

/* Read field from raw report.
 */

static int ps_input_report_reader_read(const uint8_t *report,const struct ps_input_report_field *field,char order) {
  int v;
  if (field->aligned) switch (field->c) {
    case 8: {
        v=report[field->p>>3]; 
        if (field->sign&&(v&0x80)) v|=~0x7f;
        return v;
      }
    case 16: {
        int p=field->p>>3;
        if (order=='<') {
          v=(report[p])|(report[p+1]<<8);
        } else {
          v=(report[p]<<8)|(report[p+1]);
        }
        if (field->sign&&(v&0x8000)) v|=~0x7fff;
        return v;
      }
    case 24: {
        int p=field->p>>3;
        if (order=='<') {
          v=(report[p])|(report[p+1]<<8)|(report[p+2]<<16);
        } else {
          v=(report[p]<<16)|(report[p+1]<<8)|(report[p+2]);
        }
        if (field->sign&&(v&0x800000)) v|=~0x7fffff;
        return v;
      }
    case 32: {
        int p=field->p>>3;
        if (order=='<') {
          v=(report[p])|(report[p+1]<<8)|(report[p+2]<<16)|(report[p+3]<<24);
        } else {
          v=(report[p]<<24)|(report[p+1]<<16)|(report[p+2]<<8)|(report[p+3]);
        }
        // Don't worry about (sign); it hits the sign bit either way.
        return v;
      }
    
  } else if (field->c==1) { // Single bits are both easy and likely.
    uint8_t mask;
    if (order=='<') mask=1<<(field->p&7);
    else mask=0x80>>(field->p&7);
    if (report[field->p>>3]&mask) return 1;
    return 0;
  
  } else { // Generic fallback is pretty unlikely.
    int dst=0;
    int srcp=field->p;
    int srcc=field->c;
    int bytep=srcp>>3;
    int dstshift;
    if (order=='<') {
      dstshift=0;
    } else {
      dstshift=field->c;
    }
    while (srcc>0) {
      int subp=srcp&7;
      int rdc=8-subp;
      if (rdc>srcc) rdc=srcc;
      int subv;
      if (order=='<') {
        subv=report[bytep]>>subp;
      } else {
        subv=report[bytep]>>(8-subp-rdc);
        dstshift-=rdc;
      }
      subv&=(1<<rdc)-1;
      dst|=(subv<<dstshift);
      if (order=='<') {
        dstshift+=rdc;
      }
      srcp+=rdc;
      srcc-=rdc;
      bytep++;
    }
    if (field->sign) {
      if (dst&(1<<(field->c-1))) dst|=~((1<<field->c)-1);
    }
    return dst;
  }
  return 0;
}

/* Examination.
 */

int ps_input_report_reader_is_ready(const struct ps_input_report_reader *reader) {
  if (!reader) return 0;
  return reader->ready;
}

int ps_input_report_reader_report_length(const struct ps_input_report_reader *reader) {
  if (!reader) return 0;
  return reader->reportc;
}

int ps_input_report_reader_count_fields(const struct ps_input_report_reader *reader) {
  if (!reader) return 0;
  return reader->fieldc;
}

int ps_input_report_reader_fldix_by_fldid(const struct ps_input_report_reader *reader,int fldid) {
  if (!reader) return -1;
  const struct ps_input_report_field *field=reader->fieldv;
  int fldix=0;
  for (;fldix<reader->fieldc;fldix++,field++) {
    if (field->fldid==fldid) return fldix;
  }
  return -1;
}

int ps_input_report_reader_fldid_by_fldix(const struct ps_input_report_reader *reader,int fldix) {
  if (!reader) return 0;
  if ((fldix<0)||(fldix>=reader->fieldc)) return 0;
  return reader->fieldv[fldix].fldid;
}
  
int ps_input_report_reader_position_by_fldix(const struct ps_input_report_reader *reader,int fldix) {
  if (!reader) return 0;
  if ((fldix<0)||(fldix>=reader->fieldc)) return 0;
  return reader->fieldv[fldix].p;
}
  
int ps_input_report_reader_size_by_fldix(const struct ps_input_report_reader *reader,int fldix) {
  if (!reader) return 0;
  if ((fldix<0)||(fldix>=reader->fieldc)) return 0;
  return reader->fieldv[fldix].c;
}

int ps_input_report_reader_sign_by_fldix(const struct ps_input_report_reader *reader,int fldix) {
  if (!reader) return 0;
  if ((fldix<0)||(fldix>=reader->fieldc)) return 0;
  return reader->fieldv[fldix].sign;
}

int ps_input_report_reader_value_by_fldix(const struct ps_input_report_reader *reader,int fldix) {
  if (!reader) return 0;
  if (!reader->ready) return 0;
  if ((fldix<0)||(fldix>=reader->fieldc)) return 0;
  return ps_input_report_reader_read(reader->report,reader->fieldv+fldix,reader->order);
}

/* Process single report item by accessor.
 */

static int ps_input_report_accessor_update(
  struct ps_input_report_reader *reader,
  const struct ps_input_report_accessor *accessor,
  const uint8_t *report,
  const uint8_t *previous,
  int (*cb)(struct ps_input_report_reader *reader,int fldix,int fldid,int value,int prevvalue,void *userdata),
  void *userdata
) {
  switch (accessor->type) {
  
    case PS_INPUT_REPORT_ACCESSOR_GENERIC: {
        int nv=ps_input_report_reader_read(report,&accessor->generic,reader->order);
        int pv=ps_input_report_reader_read(previous,&accessor->generic,reader->order);
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->generic.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_U8: {
        int nv=report[accessor->aligned.p];
        int pv=previous[accessor->aligned.p];
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_S8: {
        int nv=report[accessor->aligned.p];
        int pv=previous[accessor->aligned.p];
        if (nv&0x80) nv|=~0x7f;
        if (pv&0x80) pv|=~0x7f;
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_S16LE: {
        int nv=(report[accessor->aligned.p])|(report[accessor->aligned.p+1]<<8);
        int pv=(previous[accessor->aligned.p])|(previous[accessor->aligned.p+1]<<8);
        if (nv&0x8000) nv|=~0x7fff;
        if (pv&0x8000) pv|=~0x7fff;
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_S16BE: {
        int nv=(report[accessor->aligned.p]<<8)|(report[accessor->aligned.p+1]);
        int pv=(previous[accessor->aligned.p]<<8)|(previous[accessor->aligned.p+1]);
        if (nv&0x8000) nv|=~0x7fff;
        if (pv&0x8000) pv|=~0x7fff;
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_U16LE: {
        int nv=(report[accessor->aligned.p])|(report[accessor->aligned.p+1]<<8);
        int pv=(previous[accessor->aligned.p])|(previous[accessor->aligned.p+1]<<8);
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_U16BE: {
        int nv=(report[accessor->aligned.p]<<8)|(report[accessor->aligned.p+1]);
        int pv=(previous[accessor->aligned.p]<<8)|(previous[accessor->aligned.p+1]);
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_S32LE: {
        int nv=(report[accessor->aligned.p])|(report[accessor->aligned.p+1]<<8)|(report[accessor->aligned.p+2]<<16)|(report[accessor->aligned.p+3]<<24);
        int pv=(previous[accessor->aligned.p])|(previous[accessor->aligned.p+1]<<8)|(previous[accessor->aligned.p+2]<<16)|(previous[accessor->aligned.p+3]<<24);
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_S32BE: {
        int nv=(report[accessor->aligned.p]<<24)|(report[accessor->aligned.p+1]<<16)|(report[accessor->aligned.p+2]<<8)|(report[accessor->aligned.p+3]);
        int pv=(previous[accessor->aligned.p]<<24)|(previous[accessor->aligned.p+1]<<16)|(previous[accessor->aligned.p+2]<<8)|(previous[accessor->aligned.p+3]);
        if (nv==pv) return 0;
        return cb(reader,accessor->fldix,accessor->aligned.fldid,nv,pv,userdata);
      }

    case PS_INPUT_REPORT_ACCESSOR_BITS: {
        uint8_t newbits=report[accessor->bits.p];
        uint8_t oldbits=previous[accessor->bits.p];
        if (newbits==oldbits) return 0;
        uint8_t mask=0x01;
        int i=0; for (;i<8;i++,mask<<=1) {
          if (!accessor->bits.fldidv[i]) continue;
          int nv=(mask&newbits)?1:0;
          int pv=(mask&oldbits)?1:0;
          if (nv==pv) continue;
          int err=cb(reader,accessor->bits.fldixv[i],accessor->bits.fldidv[i],nv,pv,userdata);
          if (err) return err;
        }
      } return 0;
      
  }
  ps_log(INPUT,ERROR,"Invalid input report accessor type %d",accessor->type);
  return -1;
}

/* Process report.
 */

int ps_input_report_reader_set_report(
  struct ps_input_report_reader *reader,
  const void *src,int srcc,
  void *userdata,
  int (*cb)(struct ps_input_report_reader *reader,int fldix,int fldid,int value,int prevvalue,void *userdata)
) {
  if (!reader) return -1;
  if (!reader->ready) return -1;
  if ((srcc<0)||(srcc&&!src)) return -1;

  // Store the previous report.
  memcpy(reader->buf,reader->report,reader->reportc);

  // Copy incoming into (reader->report), truncate or zero-pad.
  if (srcc>reader->reportc) {
    memcpy(reader->report,src,reader->reportc);
  } else if (srcc<reader->reportc) {
    memcpy(reader->report,src,srcc);
    memset(reader->report+srcc,0,reader->reportc-srcc);
  } else {
    memcpy(reader->report,src,srcc);
  }

  // If a callback was provided, look for changes.
  if (cb) {
    if (memcmp(reader->report,reader->buf,reader->reportc)) {
      const struct ps_input_report_accessor *accessor=reader->accessorv;
      int i=reader->accessorc;
      for (;i-->0;accessor++) {
        int err=ps_input_report_accessor_update(reader,accessor,reader->report,reader->buf,cb,userdata);
        if (err) return err;
      }
    }
  }

  return 0;
}
