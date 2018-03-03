#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <sys/inotify.h>
#include <linux/input.h>
#include "ps_evdev.h"
#include "os/ps_log.h"

#define PS_EVDEV_PATH_DEFAULT "/dev/input"

/* Device.
 */

struct ps_evdev_abs {
  uint16_t code;
  int lo,hi;
  int value;
};

struct ps_evdev_device {
  int devid;
  int evid;
  int fd;
  int grabbed;
  struct input_id id;
  char name[64];
  uint8_t keybit[(KEY_MAX+7)>>3];
  uint8_t absbit[(ABS_MAX+7)>>3];
  uint8_t relbit[(REL_MAX+7)>>3];
  uint8_t key[(KEY_MAX+7)>>3]; // State of every possible key.
  struct ps_evdev_abs *absv; // State and range of defined absolute axes.
  int absc,absa;
};

static void ps_evdev_device_cleanup(struct ps_evdev_device *dev) {
  if (!dev) return;
  if (dev->fd>=0) close(dev->fd);
  if (dev->absv) free(dev->absv);
}

/* Globals.
 */

static struct {

  char *path;

  int infd;
  int inwd;

  int (*cb_connect)(int devid);
  int (*cb_disconnect)(int devid,int reason);
  int (*cb_event)(int devid,int type,int code,int value);

  struct ps_evdev_device *devv;
  int devc,deva;

  int callingback;
  int devid_next;

} ps_evdev={0};

/* Open device.
 * If already open or not an event device, return 0.
 * Return <0 only for serious errors.
 * Return >0 if newly opened.
 */

static int ps_evdev_try_file(const char *base) {

  /* If devid are depleted, we can't open any more. User should restart program every few billion years. */
  if (ps_evdev.devid_next<1) return 0;

  /* Basename must be /event[0-9]+/. The number ("evid") must be unique. */
  if (memcmp(base,"event",5)||!base[5]) return 0;
  int evid=0,i;
  for (i=5;base[i];i++) {
    int digit=base[i]-'0';
    if ((digit<0)||(digit>9)) return 0;
    if (evid>INT_MAX/10) return 0; evid*=10;
    if (evid>INT_MAX-digit) return 0; evid+=digit;
  }
  int basec=i;
  for (i=0;i<ps_evdev.devc;i++) if (ps_evdev.devv[i].evid==evid) return 0;

  /* Compose full path and open it. No error if it fails to open. */
  char path[1024];
  int pfxc=0; while (ps_evdev.path[pfxc]) pfxc++;
  while (pfxc&&(ps_evdev.path[pfxc-1]=='/')) pfxc--;
  if (pfxc>=sizeof(path)-basec-1) return 0;
  memcpy(path,ps_evdev.path,pfxc);
  path[pfxc]='/';
  memcpy(path+pfxc+1,base,basec+1);
  int fd=open(path,O_RDONLY);
  if (fd<0) {
    int panic=10;
    while ((fd<0)&&(errno==EINTR)&&(--panic>0)) fd=open(path,O_RDONLY);
    if (fd<0) return 0;
  }
  fcntl(fd,F_SETFD,FD_CLOEXEC);

  /* EVIOGVERSION to confirm it is really evdev. */
  int version=0;
  if (ioctl(fd,EVIOCGVERSION,&version)<0) { close(fd); return 0; }

  /* Grow device list and prepare the new one. */
  if (ps_evdev.devc>=ps_evdev.deva) {
    int na=ps_evdev.deva+8;
    if (na>INT_MAX/sizeof(struct ps_evdev_device)) { close(fd); return -1; }
    void *nv=realloc(ps_evdev.devv,sizeof(struct ps_evdev_device)*na);
    if (!nv) { close(fd); return -1; }
    ps_evdev.devv=nv;
    ps_evdev.deva=na;
  }
  struct ps_evdev_device *dev=ps_evdev.devv+ps_evdev.devc++;
  memset(dev,0,sizeof(struct ps_evdev_device));
  dev->devid=ps_evdev.devid_next;
  dev->evid=evid;
  dev->fd=fd;

  /* Read device properties. */
  ioctl(fd,EVIOCGID,&dev->id);
  ioctl(fd,EVIOCGNAME(sizeof(dev->name)),dev->name);
  dev->name[sizeof(dev->name)-1]=0;
  ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(dev->keybit)),dev->keybit);
  ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(dev->absbit)),dev->absbit);
  ioctl(fd,EVIOCGBIT(EV_REL,sizeof(dev->relbit)),dev->relbit);

  /* Read range and initial value of absolute axes. */
  int major; for (major=0;major<sizeof(dev->absbit);major++) {
    if (!dev->absbit[major]) continue;
    int minor; for (minor=0;minor<8;minor++) {
      if (!(dev->absbit[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      struct input_absinfo info={0};
      if (ioctl(fd,EVIOCGABS(code),&info)<0) continue;
      if (dev->absc>=dev->absa) {
        int na=dev->absa+8;
        if (na>INT_MAX/sizeof(struct ps_evdev_abs)) { ps_evdev_device_cleanup(dev); ps_evdev.devc--; return -1; }
        void *nv=realloc(dev->absv,sizeof(struct ps_evdev_abs)*na);
        if (!nv) { ps_evdev_device_cleanup(dev); ps_evdev.devc--; return -1; }
        dev->absv=nv;
        dev->absa=na;
      }
      struct ps_evdev_abs *abs=dev->absv+dev->absc++;
      abs->code=code;
      abs->lo=info.minimum;
      abs->hi=info.maximum;
      abs->value=info.value;
      if (abs->lo>abs->hi) abs->hi=abs->lo;
      if (abs->value<abs->lo) abs->value=abs->lo;
      else if (abs->value>abs->hi) abs->value=abs->hi;
    }
  }

  /* Try to grab it. */
  if (ioctl(fd,EVIOCGRAB,1)>=0) {
    dev->grabbed=1;
  }

  ps_evdev.devid_next++;

  /* Notify user. */
  if (ps_evdev.cb_connect) {
    ps_evdev.callingback=1;
    int err=ps_evdev.cb_connect(dev->devid);
    ps_evdev.callingback=0;
    if (err<0) { ps_evdev_device_cleanup(dev); ps_evdev.devc--; return -1; }
  }

  return 1;
}

/* Update device.
 */

static int ps_evdev_abs_search(const struct ps_evdev_device *dev,uint16_t code) {
  int lo=0,hi=dev->absc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (code<dev->absv[ck].code) hi=ck;
    else if (code>dev->absv[ck].code) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ps_evdev_device_event(struct ps_evdev_device *dev,struct input_event *evt) {
  switch (evt->type) {

    case EV_SYN: return 0;

    case EV_ABS: {
        int p=ps_evdev_abs_search(dev,evt->code);
        if (p>=0) {
          struct ps_evdev_abs *abs=dev->absv+p;
          if (evt->value<abs->lo) evt->value=abs->lo;
          else if (evt->value>abs->hi) evt->value=abs->hi;
          if (evt->value==abs->value) return 0;
          abs->value=evt->value;
        }
        if (ps_evdev.cb_event) return ps_evdev.cb_event(dev->devid,EV_ABS,evt->code,evt->value);
      } break;

    case EV_KEY: {
        if ((evt->code>=0)&&(evt->code<=KEY_MAX)) {
          int major=evt->code>>3;
          uint8_t mask=1<<(evt->code&7);
          if (evt->value) dev->key[major]|=mask;
          else dev->key[major]&=~mask;
        }
        if (ps_evdev.cb_event) return ps_evdev.cb_event(dev->devid,EV_KEY,evt->code,evt->value);
      } break;

    default: if (ps_evdev.cb_event) return ps_evdev.cb_event(dev->devid,evt->type,evt->code,evt->value);
  }
  return 0;
}

static int ps_evdev_update_device(struct ps_evdev_device *dev) {
  struct input_event evtv[16];
  int evtc=read(dev->fd,evtv,sizeof(evtv));
  if (evtc<0) {
    if (errno==EINTR) return 1;
    return -1;
  }
  if (!evtc) return 0;
  evtc/=sizeof(struct input_event);
  int i; for (i=0;i<evtc;i++) {
    int err=ps_evdev_device_event(dev,evtv+i);
    if (err<0) return err;
  }
  return 1;
}

/* Update inotify.
 */

static int ps_evdev_update_inotify() {
  char buf[1024];
  int bufp=0,bufc=0;
  if ((bufc=read(ps_evdev.infd,buf,sizeof(buf)))<=0) {
    if ((bufc<0)&&(errno==EINTR)) return 1;
    return bufc;
  }
  while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *evt=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-evt->len) return -1;
    if (evt->len<1) return -1;
    evt->name[evt->len-1]=0; // Just to be safe....
    bufp+=evt->len;
    if (evt->mask&(IN_CREATE|IN_ATTRIB)) {
      int err=ps_evdev_try_file(evt->name);
      if (err<0) return err;
    }
  }
  if (bufp<bufc) return -1;
  return 1;
}

/* Scan for devices.
 */

static int ps_evdev_scan() {
  DIR *dir=opendir(ps_evdev.path);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    int err=ps_evdev_try_file(de->d_name);
    if (err<0) { closedir(dir); return err; }
  }
  closedir(dir);
  return 0;
}

/* Init.
 */

int ps_evdev_init(
  const char *path,
  int (*cb_connect)(int devid),
  int (*cb_disconnect)(int devid,int reason),
  int (*cb_event)(int devid,int type,int code,int value)
) {
  if (ps_evdev.path) return -1;
  memset(&ps_evdev,0,sizeof(ps_evdev));

  if (!path) path=PS_EVDEV_PATH_DEFAULT;
  if (!(ps_evdev.path=strdup(path))) return -1;

  ps_evdev.cb_connect=cb_connect;
  ps_evdev.cb_disconnect=cb_disconnect;
  ps_evdev.cb_event=cb_event;
  ps_evdev.devid_next=1;

  if ((ps_evdev.infd=inotify_init())<0) { ps_evdev_quit(); return -1; }
  fcntl(ps_evdev.infd,F_SETFD,FD_CLOEXEC);
  if ((ps_evdev.inwd=inotify_add_watch(ps_evdev.infd,ps_evdev.path,IN_CREATE|IN_ATTRIB))<0) { ps_evdev_quit(); return -1; }

  if (ps_evdev_scan()<0) { ps_evdev_quit(); return -1; }
  
  return 0;
}

/* Quit.
 */

void ps_evdev_quit() {
  if (ps_evdev.callingback) {
    fprintf(stderr,"ps_evdev:PANIC: ps_evdev_quit() called during event callback!\n");
    return;
  }
  if (ps_evdev.path) {
    free(ps_evdev.path);
    if (ps_evdev.infd>=0) {
      if (ps_evdev.inwd>=0) inotify_rm_watch(ps_evdev.infd,ps_evdev.inwd);
      close(ps_evdev.infd);
    }
  }
  if (ps_evdev.devv) {
    while (ps_evdev.devc-->0) ps_evdev_device_cleanup(ps_evdev.devv+ps_evdev.devc);
    free(ps_evdev.devv);
  }
  memset(&ps_evdev,0,sizeof(ps_evdev));
}

/* Update.
 */

int ps_evdev_update() {
  if (!ps_evdev.path) return -1;
  if (ps_evdev.callingback) return -1;
  struct timeval to={0};
  fd_set fdr;
  int i,do_select=0;
  FD_ZERO(&fdr);

  if (ps_evdev.infd>=0) {
    FD_SET(ps_evdev.infd,&fdr);
    do_select=1;
  }
  for (i=0;i<ps_evdev.devc;i++) {
    struct ps_evdev_device *dev=ps_evdev.devv+i;
    FD_SET(dev->fd,&fdr);
    do_select=1;
  }

  if (do_select&&(select(FD_SETSIZE,&fdr,0,0,&to)>0)) {
    if ((ps_evdev.infd>=0)&&FD_ISSET(ps_evdev.infd,&fdr)) {
      int err=ps_evdev_update_inotify();
      if (err<=0) {
        inotify_rm_watch(ps_evdev.infd,ps_evdev.inwd);
        close(ps_evdev.infd);
        if (ps_evdev.cb_disconnect) {
          ps_evdev.callingback=1;
          err=ps_evdev.cb_disconnect(0,err);
          ps_evdev.callingback=0;
        }
        if (err<0) return err;
      }
    }
    for (i=0;i<ps_evdev.devc;) {
      struct ps_evdev_device *dev=ps_evdev.devv+i;
      if (!FD_ISSET(dev->fd,&fdr)) { i++; continue; }
      int err=ps_evdev_update_device(dev);
      if (err<=0) {
        if (ps_evdev.cb_disconnect) {
          ps_evdev.callingback=1;
          err=ps_evdev.cb_disconnect(dev->devid,err);
          ps_evdev.callingback=0;
        }
        ps_evdev_device_cleanup(dev);
        ps_evdev.devc--;
        memmove(ps_evdev.devv+i,ps_evdev.devv+i+1,sizeof(struct ps_evdev_device)*(ps_evdev.devc-i));
        if (err<0) return err;
      } else i++;
    }
  }

  return 0;
}

/* Public accessors.
 */

int ps_evdev_dev_search(int devid) {
  int lo=0,hi=ps_evdev.devc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<ps_evdev.devv[ck].devid) hi=ck;
    else if (devid>ps_evdev.devv[ck].devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ps_evdev_count_devices() {
  return ps_evdev.devc;
}

int ps_evdev_devid_for_index(int index) {
  if ((index<0)||(index>=ps_evdev.devc)) return -1;
  return ps_evdev.devv[index].devid;
}

int ps_evdev_devid_for_evid(int evid) {
  int i; for (i=0;i<ps_evdev.devc;i++) if (ps_evdev.devv[i].evid==evid) return ps_evdev.devv[i].devid;
  return -1;
}

const char *ps_evdev_get_name(int devid) {
  int p=ps_evdev_dev_search(devid);
  if (p<0) return 0;
  return ps_evdev.devv[p].name;
}

int ps_evdev_get_id(int *bustype,int *vendor,int *product,int *version,int devid) {
  int p=ps_evdev_dev_search(devid);
  if (p<0) return 0;
  if (bustype) *bustype=ps_evdev.devv[p].id.bustype;
  if (vendor) *vendor=ps_evdev.devv[p].id.vendor;
  if (product) *product=ps_evdev.devv[p].id.product;
  if (version) *version=ps_evdev.devv[p].id.version;
  return ps_evdev.devv[p].evid;
}

int ps_evdev_get_abs(int *lo,int *hi,int devid,int code) {
  if ((code<0)||(code>ABS_MAX)) return 0;
  int p=ps_evdev_dev_search(devid);
  if (p<0) return 0;
  struct ps_evdev_device *dev=ps_evdev.devv+p;
  if ((p=ps_evdev_abs_search(dev,code))<0) return 0;
  if (lo) *lo=dev->absv[p].lo;
  if (hi) *hi=dev->absv[p].hi;
  return dev->absv[p].value;
}

int ps_evdev_get_key(int devid,int code) {
  if ((code<0)||(code>KEY_MAX)) return 0;
  int p=ps_evdev_dev_search(devid);
  if (p<0) return 0;
  return (ps_evdev.devv[p].key[code>>3]&(1<<(code&7)))?1:0;
}

int ps_evdev_has_key(int devid,int code) {
  if ((code<0)||(code>KEY_MAX)) return 0;
  int p=ps_evdev_dev_search(devid);
  if (p<0) return 0;
  return (ps_evdev.devv[p].keybit[code>>3]&(1<<(code&7)))?1:0;
}

int ps_evdev_has_rel(int devid,int code) {
  if ((code<0)||(code>REL_MAX)) return 0;
  int p=ps_evdev_dev_search(devid);
  if (p<0) return 0;
  return (ps_evdev.devv[p].relbit[code>>3]&(1<<(code&7)))?1:0;
}

/* Report all device capabilities.
 */

int ps_evdev_report_capabilities(int devid,int (*cb)(int devid,int type,int code,int lo,int hi,int value,void *userdata),void *userdata) {
  if (!cb) return -1;
  int p=ps_evdev_dev_search(devid);
  if (p<0) return -1;
  struct ps_evdev_device *dev=ps_evdev.devv+p;
  int major,minor,err;
  for (major=0;major<sizeof(dev->keybit);major++) {
    if (!dev->keybit[major]) continue;
    for (minor=0;minor<8;minor++) {
      if (!(dev->keybit[major]&(1<<minor))) continue;
      if (err=cb(dev->devid,EV_KEY,(major<<3)|minor,0,1,(dev->key[major]&(1<<minor))?1:0,userdata)) return err;
    }
  }
  int absp=0;
  for (major=0;major<sizeof(dev->absbit);major++) {
    if (!dev->absbit[major]) continue;
    for (minor=0;minor<8;minor++) {
      if (!(dev->absbit[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      while ((absp<dev->absc)&&(dev->absv[absp].code<code)) absp++;
      int value=0,lo=INT_MIN,hi=INT_MAX;
      if ((absp<dev->absc)&&(dev->absv[absp].code==code)) {
        value=dev->absv[absp].value;
        lo=dev->absv[absp].lo;
        hi=dev->absv[absp].hi;
      }
      if (err=cb(dev->devid,EV_ABS,code,lo,hi,value,userdata)) return err;
    }
  }
  for (major=0;major<sizeof(dev->relbit);major++) {
    if (!dev->relbit[major]) continue;
    for (minor=0;minor<8;minor++) {
      if (!(dev->relbit[major]&(1<<minor))) continue;
      if (err=cb(dev->devid,EV_REL,(major<<3)|minor,INT_MIN,INT_MAX,0,userdata)) return err;
    }
  }
  return 0;
}
