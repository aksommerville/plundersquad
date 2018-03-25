#include "ps.h"
#include "ps_input_report_reader.h"

/* Longest allowable report, in bits.
 * Anything in this ballpark must be a mistake.
 */
#define PS_INPUT_REPORT_SANITY_LIMIT 65536

/* Object definition.
 */

struct ps_input_report_field {
  int p,c; // Position and size in bits.
  int sign;
  int aligned; // Hint that this field can be read as bytes.
  int fldid;
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
    reader->reportc=0;
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
  // TODO This comparison could be more efficient. Especially with bitfields; it seems wasteful to examine each bit individually.
  if (cb) {
    if (memcmp(reader->report,reader->buf,reader->reportc)) {
      const struct ps_input_report_field *field=reader->fieldv;
      int fldix=0;
      for (;fldix<reader->fieldc;fldix++,field++) {
        int nv=ps_input_report_reader_read(reader->report,field,reader->order);
        int pv=ps_input_report_reader_read(reader->buf,field,reader->order);
        if (nv==pv) continue;
        int err=cb(reader,fldix,field->fldid,nv,pv,userdata);
        if (err) return err;
      }
    }
  }

  return 0;
}
