#include "akau_store_internal.h"
#include "akau_internal.h"
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <zlib.h>

/* Text helpers.
 */

static int akau_count_newlines(const char *src,int srcc) {
  int nlc=0,srcp=0;
  while (srcp<srcc) {
    if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      nlc++;
      if (++srcp>=srcc) return nlc;
      if ((src[srcp]!=src[srcp-1])&&((src[srcp]==0x0a)||(src[srcp]==0x0d))) {
        srcp++;
      }
    } else srcp++;
  }
  return nlc;
}

static int akau_is_text(const char *src,int srcc) {
  int srcp=srcc; while (srcp-->0) {
    if ((src[srcp]>=0x20)&&(src[srcp]<=0x7e)) continue;
    if (src[srcp]==0x0a) continue;
    if (src[srcp]==0x0d) continue;
    if (src[srcp]==0x09) continue;
    return 0;
  }
  return 1;
}

/* Decode ipcm or fpcm. The decoder we call detects its format for real.
 */

static void *akau_store_decode_ipcm(uint8_t resfmt,const void *src,int srcc) {
  switch (resfmt) {
    case 1: case 2: return akau_ipcm_decode(src,srcc);
    case 6: return akau_wavegen_decode(src,srcc);
  }
  return 0;
}

static void *akau_store_decode_fpcm(uint8_t resfmt,const void *src,int srcc) {
  switch (resfmt) {
    case 1: case 2: return akau_fpcm_decode(src,srcc);
    case 5: return akau_decode_fpcm_harmonics(0,src,srcc,1);
    case 6: {
        struct akau_ipcm *ipcm=akau_wavegen_decode(src,srcc);
        struct akau_fpcm *fpcm=akau_fpcm_from_ipcm(ipcm,32767);
        akau_ipcm_del(ipcm);
        return fpcm;
      }
  }
  return 0;
}

/* Decode instrument. Select decoder for format.
 */

static void *akau_store_decode_instrument(uint8_t resfmt,const void *src,int srcc) {
  struct akau_instrument *instrument=0;
  switch (resfmt) {
  
    case 1: case 2: { // PCM types -- decode, then wrap in an instrument with sensible defaults.
        struct akau_fpcm *fpcm=akau_fpcm_decode(src,srcc);
        if (!fpcm) return 0;
        instrument=akau_instrument_new_default(fpcm);
        akau_fpcm_del(fpcm);
      } break;

    case 5: {
        struct akau_fpcm *fpcm=akau_decode_fpcm_harmonics(0,src,srcc,1);
        if (!fpcm) return 0;
        instrument=akau_instrument_new_default(fpcm);
        akau_fpcm_del(fpcm);
      } break;
      
    case 4: { // Instrument text.
        instrument=akau_instrument_decode(src,srcc);
      } break;
      
  }
  if (!instrument) return 0;
  return instrument;
}

/* Decode song. Select decoder for format.
 */

static void akau_store_decode_song_text_error(const char *msg,int msgc,const char *src,int srcc,int srcp) {
  int lineno=akau_count_newlines(src,srcp)+1;
  akau_error("%d: %.*s",lineno,msgc,msg);
}

static void *akau_store_decode_song(uint8_t resfmt,const void *src,int srcc) {
  switch (resfmt) {
    case 3: {
        struct akau_song *song=akau_song_new();
        if (!song) return 0;
        if (akau_song_decode(song,src,srcc)<0) {
          akau_song_del(song);
          return 0;
        }
        return song;
      }
    default: {
        // Format not appropriate for song resource type.
        return 0;
      }
  }
  return 0;
}

/* Decode and load one resource.
 */

static int akau_store_decode_resource(struct akau_store *store,uint8_t restype,uint8_t resfmt,int resid,const void *src,int srcc) {

  /* Locate store and decoder based on (restype). */
  struct akau_store_list *list;
  void *(*decode)(uint8_t resfmt,const void *src,int srcc);
  switch (restype) {
    case 1: list=&store->ipcms; decode=akau_store_decode_ipcm; break;
    case 2: list=&store->fpcms; decode=akau_store_decode_fpcm; break;
    case 3: list=&store->instruments; decode=akau_store_decode_instrument; break;
    case 4: list=&store->songs; decode=akau_store_decode_song; break;
    default: {
        akau_error("Unknown resource type %d.",restype);
        return -1;
      }
  }

  /* Locate insertion point, ensure valid and unused ID. */
  if (resid<1) return -1;
  int p=akau_store_list_search(list,resid);
  if (p>=0) {
    akau_error("Duplicate resource %d:%d.",restype,resid);
    return -1;
  }
  p=-p-1;

  /* Decode resource. */
  void *obj=decode(resfmt,src,srcc);
  if (!obj) {
    return -1;
  }

  /* Insert to store. */
  if (akau_store_list_insert(list,p,resid,obj)<0) {
    if (list->del) list->del(obj);
    return -1;
  }
  if (list->del) list->del(obj);
  
  return 0;
}

/* Read complete file.
 */

static int akau_read_file(void *dstpp,const char *path) {
  if (!dstpp||!path) return -1;
  int mode=O_RDONLY;
  #if PS_ARCH==PS_ARCH_mswin
    mode|=O_BINARY;
  #endif
  int fd=open(path,mode);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Compressed file reader.
 */

struct akau_zreader {
  int fd;
  z_stream z;
  int zinit;
  char cbuf[8192];
  int cbufp,cbufc;
  char *dbuf;
  int dbufp,dbufc,dbufa;
};

static void akau_zreader_cleanup(struct akau_zreader *zreader) {
  if (!zreader) return;
  if (zreader->fd>0) close(zreader->fd);
  if (zreader->zinit) inflateEnd(&zreader->z);
  if (zreader->dbuf) free(zreader->dbuf);
}

static int akau_zreader_dbuf_require(struct akau_zreader *zreader,int addc) {
  if (!zreader) return -1;
  if (addc<1) return 0;
  if (zreader->dbufc>INT_MAX-addc) return -1;
  int na=zreader->dbufc+addc;
  if (na<=zreader->dbufa-zreader->dbufp) return 0;
  if (zreader->dbufp) {
    memmove(zreader->dbuf,zreader->dbuf+zreader->dbufp,zreader->dbufc);
    zreader->dbufp=0;
    if (zreader->dbufc+addc<=zreader->dbufa) return 0;
  }
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(zreader->dbuf,na);
  if (!nv) return -1;
  zreader->dbuf=nv;
  zreader->dbufa=na;
  return 0;
}

/* Open compressed file.
 */

static int akau_zreader_open(struct akau_zreader *zreader,const char *path) {
  zreader->fd=open(path,O_RDONLY);
  if (zreader->fd<0) return -1;
  if (inflateInit(&zreader->z)<0) return -1;
  zreader->zinit=1;
  return 0;
}

/* Refill zreader.
 * Returns >0 if content was added to cbuf, or 0 if finished.
 */

static int akau_zreader_read_file(struct akau_zreader *zreader) {
  if (zreader->fd<0) return 0;
  if (zreader->cbufc>=sizeof(zreader->cbuf)) return -1;
  if (zreader->cbufp+zreader->cbufc>=sizeof(zreader->cbuf)) {
    memmove(zreader->cbuf,zreader->cbuf+zreader->cbufp,zreader->cbufc);
    zreader->cbufp=0;
  }
  int err=read(zreader->fd,zreader->cbuf,sizeof(zreader->cbuf)-zreader->cbufc-zreader->cbufp);
  if (err<0) return -1;
  if (!err) {
    close(zreader->fd);
    zreader->fd=-1;
    return 0;
  }
  zreader->cbufc+=err;
  return err;
}

/* Read compressed file and decompress until we have received at least the requested amount.
 */

static int akau_zreader_read(struct akau_zreader *zreader,int expectc) {
  if (zreader->dbufc>=expectc) return 0;
  if (akau_zreader_dbuf_require(zreader,expectc-zreader->dbufc)<0) return -1;
  while (zreader->dbufc<expectc) {

    /* If we have any compressed content, try to decompress. */
    if (zreader->cbufc) {
      zreader->z.next_in=(Bytef*)zreader->cbuf+zreader->cbufp;
      zreader->z.avail_in=zreader->cbufc;
      zreader->z.next_out=(Bytef*)zreader->dbuf+zreader->dbufp+zreader->dbufc;
      int avail_out_0=zreader->dbufa-zreader->dbufc-zreader->dbufp;
      zreader->z.avail_out=avail_out_0;
      if (inflate(&zreader->z,Z_NO_FLUSH)<0) {
        if (inflate(&zreader->z,Z_SYNC_FLUSH)<0) {
          return -1;
        }
      }
      int addc=avail_out_0-zreader->z.avail_out;
      zreader->dbufc+=addc;
      addc=zreader->cbufc-zreader->z.avail_in;
      zreader->cbufp+=addc;
      zreader->cbufc-=addc;
      if (zreader->dbufc>=expectc) break;
    }

    /* Read some more off the file. */
    if (akau_zreader_read_file(zreader)<=0) return -1;
    
  }
  return 0;
}

/* Read, decompress, and load one resource.
 */

static int akau_store_load_file_resource(struct akau_store *store,struct akau_zreader *zreader) {

  /* Acquire header. */
  if (akau_zreader_read(zreader,8)<0) {
    if ((zreader->fd<0)&&!zreader->cbufc&&!zreader->dbufc) {
      // End of file.
      return 0;
    }
    return akau_error("Failed to read compressed file.");
  }

  /* Read header. */
  uint8_t *src=(uint8_t*)(zreader->dbuf+zreader->dbufp);
  uint8_t restype=src[0];
  uint8_t resfmt=src[1];
  uint16_t resid=(src[2]<<8)|src[3];
  uint32_t dlen=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  if (!resid) return -1;
  if (dlen>=0x10000000) return -1;
  zreader->dbufp+=8;
  zreader->dbufc-=8;

  /* Read resource body. */
  if (akau_zreader_read(zreader,dlen)<0) return akau_error("Failed to read compressed file.");

  /* Deliver to store. */
  if (akau_store_decode_resource(store,restype,resfmt,resid,zreader->dbuf+zreader->dbufp,dlen)<0) return -1;
  zreader->dbufp+=dlen;
  zreader->dbufc-=dlen;

  return 1;
}

/* Load store from regular file (archive).
 */

static int akau_store_load_file(struct akau_store *store,const char *path) {
  struct akau_zreader zreader={0};
  if (akau_zreader_open(&zreader,path)<0) {
    akau_zreader_cleanup(&zreader);
    return akau_error("%s: Open failed.",path);
  }

  if (
    (akau_zreader_read(&zreader,8)<0)||
    memcmp(zreader.dbuf,"\0AKAUAR\xff",8)
  ) {
    akau_zreader_cleanup(&zreader);
    return akau_error("%s: Failed to verify archive signature.",path);
  }
  zreader.dbufp+=8;
  zreader.dbufc-=8;

  while (1) {
    int err=akau_store_load_file_resource(store,&zreader);
    if (err<=0) {
      akau_zreader_cleanup(&zreader);
      return err;
    }
  }
}

/* Guess format of resource.
 */

static int akau_store_guess_format(const char *path,int restype,const void *src,int srcc) {

  /* Trust the path extension if present and known. */
  const char *ext=0;
  int extc=0,pathp=0;
  while (path[pathp]) {
    if (path[pathp]=='/') {
      ext=0;
      extc=0;
    } else if (path[pathp]=='.') {
      ext=path+pathp+1;
      extc=0;
    } else if (ext) {
      extc++;
    }
    pathp++;
  }
  if ((extc>0)&&(extc<16)) {
    char lext[16];
    int i; for (i=0;i<extc;i++) {
      if ((ext[i]>='A')&&(ext[i]<='Z')) lext[i]=ext[i]+0x20;
      else lext[i]=ext[i];
    }
    switch (extc) {
      case 3: {
          if (!memcmp(lext,"wav",3)) return 1;
          if (!memcmp(lext,"ins",3)) return 3;
        } break;
      case 4: {
          if (!memcmp(lext,"song",4)) return 3;
        } break;
      case 5: {
          if (!memcmp(lext,"akpcm",5)) return 2;
        } break;
      case 7: {
          if (!memcmp(lext,"wavegen",7)) return 6;
        } break;
      case 9: {
          if (!memcmp(lext,"harmonics",9)) return 5;
        } break;
    }
  }

  /* Consider content signatures. */
  if ((srcc>=12)&&!memcmp(src,"RIFF",4)&&!memcmp((char*)src+8,"WAVE",4)) return 1;
  if ((srcc>=8)&&!memcmp(src,"\0AKAUPCM",8)) return 2;

  /* Text formats generally don't have a signature. Assume a default for each type. */
  switch (restype) {
    case 1: return 2; // ipcm; decoder guesses on its own
    case 2: { // fpcm; small text files assume 'harmonics', otherwise let the decoder guess.
        if ((srcc<=1024)&&akau_is_text(src,srcc)) return 5;
        return 2;
      } break;
    case 3: return 4; // instrument; we only have one complete format
    case 4: return 3; // song; we only have one format
  }

  return -1;
}

/* Load single resource from regular file.
 */

static int akau_store_load_typed_file(struct akau_store *store,const char *path,int restype,int resid) {

  void *src=0;
  int srcc=akau_read_file(&src,path);
  if ((srcc<0)||!src) return akau_error("%s: Failed to read file.",path);

  int resfmt=akau_store_guess_format(path,restype,src,srcc);
  if (resfmt<0) {
    akau_error("%s: Unable to determine file format.",path);
    free(src);
    return -1;
  }

  if (akau_store_decode_resource(store,restype,resfmt,resid,src,srcc)<0) {
    akau_error("%s: Failed to decode resource.",path);
    free(src);
    return -1;
  }

  free(src);
  return 0;
}

/* Load single list from directory.
 */

static int akau_store_load_typed_directory(struct akau_store *store,const char *path,int pathc,int restype) {

  char subpath[1024];
  if (pathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,pathc);
  subpath[pathc++]='/';

  DIR *dir=opendir(path);
  if (!dir) return akau_error("%s: Failed to read directory.",path);
  struct dirent *de;
  while (de=readdir(dir)) {

    const char *base=de->d_name;
    int basec=0; while (base[basec]) basec++;
    if (pathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }
    
    int id=0,basep=0;
    while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
      int digit=base[basep++]-'0';
      if (id>INT_MAX/10) { basep=0; break; }
      id*=10;
      if (id>INT_MAX-digit) { basep=0; break; }
      id+=digit;
    }
    if (!basep) continue;

    memcpy(subpath+pathc,base,basec+1);
    if (akau_store_load_typed_file(store,subpath,restype,id)<0) {
      closedir(dir);
      return -1;
    }
    
  }
  closedir(dir);
  return 0;
}

/* Load store from directory.
 */

static int akau_store_load_directory(struct akau_store *store,const char *path) {

  char subpath[1024];
  int pathc=0; while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,pathc);
  subpath[pathc++]='/';

  DIR *dir=opendir(path);
  if (!dir) return akau_error("%s: Failed to read directory.",path);
  struct dirent *de;
  while (de=readdir(dir)) {
  
    const char *base=de->d_name;
    int basec=0; while (base[basec]) basec++;
    if (pathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }

    int restype;
    if ((basec==4)&&!memcmp(base,"ipcm",4)) restype=1;
    else if ((basec==4)&&!memcmp(base,"fpcm",4)) restype=2;
    else if ((basec==10)&&!memcmp(base,"instrument",10)) restype=3;
    else if ((basec==4)&&!memcmp(base,"song",4)) restype=4;
    else continue;

    memcpy(subpath+pathc,base,basec+1);
    if (akau_store_load_typed_directory(store,subpath,pathc+basec,restype)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Link resources with deferred external references.
 */

static int akau_store_link(struct akau_store *store) {
  if (!store) return -1;
  
  int i=store->songs.c; for (;i-->0;) {
    struct akau_song *song=store->songs.v[i].obj;
    if (akau_song_link(song,store)<0) {
      akau_error("Failed to link song ID %d.",store->songs.v[i].id);
      return -1;
    }
  }

  for (i=store->instruments.c;i-->0;) {
    struct akau_instrument *instrument=store->instruments.v[i].obj;
    if (akau_instrument_link(instrument,store)<0) {
      akau_error("Failed to link instrument ID %d.",store->instruments.v[i].id);
      return -1;
    }
  }
  
  return 0;
}

/* Load into store, main entry point.
 */
 
int akau_store_load(struct akau_store *store,const char *path) {
  if (!store||!path) return -1;

  struct stat st={0};
  if (stat(path,&st)<0) return -1;
  if (S_ISDIR(st.st_mode)) {
    if (akau_store_load_directory(store,path)<0) return -1;
  } else if (S_ISREG(st.st_mode)) {
    if (akau_store_load_file(store,path)<0) return -1;
  } else {
    return akau_error("%s: Unknown file type.",path);
  }

  if (store->path) free(store->path);
  store->path=strdup(path);

  if (akau_store_link(store)<0) return -1;
  
  return 0;
}

/* Get path to resource file.
 */
 
int akau_store_get_resource_path(char *dst,int dsta,const struct akau_store *store,const char *restype,int resid) {
  if ((dsta<0)||(dsta&&!dst)) return -1;
  if (!store) return -1;
  if (!store->path) return -1;
  if (!restype) return -1;
  if (resid<1) return -1;

  struct stat st={0};
  if (stat(store->path,&st)<0) return -1;
  if (!S_ISDIR(st.st_mode)) return -1;

  char dirpath[1024];
  int dirpathc=snprintf(dirpath,sizeof(dirpath),"%s/%s/",store->path,restype);
  if ((dirpathc<0)||(dirpathc>=sizeof(dirpath))) return -1;
  
  DIR *dir=opendir(dirpath);
  if (!dir) return akau_error("%s: Failed to read directory.",dirpath);
  struct dirent *de;
  while (de=readdir(dir)) {

    const char *base=de->d_name;
    int basec=0; while (base[basec]) basec++;
    
    int id=0,basep=0;
    while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
      int digit=base[basep++]-'0';
      if (id>INT_MAX/10) { basep=0; break; }
      id*=10;
      if (id>INT_MAX-digit) { basep=0; break; }
      id+=digit;
    }
    if (!basep) continue;

    if (id==resid) {
      closedir(dir);
      int dstc=dirpathc+basec;
      if (dstc<=dsta) {
        memcpy(dst,dirpath,dirpathc);
        memcpy(dst+dirpathc,base,basec);
        if (dstc<dsta) dst[dstc]=0;
      }
      return dstc;
    }
  }
  closedir(dir);

  return -1;
}

/* Compose default path to resource file.
 */
 
int akau_store_generate_resource_path(char *dst,int dsta,const struct akau_store *store,const char *restype,int resid) {
  if ((dsta<0)||(dsta&&!dst)) return -1;
  if (!store) return -1;
  if (!store->path) return -1;
  if (!restype) return -1;
  if (resid<1) return -1;

  struct stat st={0};
  if (stat(store->path,&st)<0) return -1;
  if (!S_ISDIR(st.st_mode)) return -1;

  int dstc=snprintf(dst,dsta,"%s/%s/%03d",store->path,restype,resid);
  if ((dstc<0)||(dstc>=dsta)) return -1;
  return dstc;
}
