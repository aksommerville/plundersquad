#include "test/ps_test.h"
#include "os/ps_userconfig.h"
#include "util/ps_buffer.h"

/* Shared test data.
 */

static struct ps_userconfig *ps_new_userconfig_with_common_fields() {
  struct ps_userconfig *userconfig=ps_userconfig_new();
  if (!userconfig) return 0;
  if (ps_userconfig_declare_boolean(userconfig,"enable",0)<0) return 0;
  if (ps_userconfig_declare_integer(userconfig,"value",0,-100,100)<0) return 0;
  if (ps_userconfig_declare_string(userconfig,"name","",0)<0) return 0;
  if (ps_userconfig_declare_path(userconfig,"path","",0,0)<0) return 0;
  return userconfig;
}

/* Create a userconfig, add a bunch of fields to it, and confirm that we can read them back.
 */

PS_TEST(test_userconfig_definition,userconfig) {
  struct ps_userconfig *userconfig=ps_userconfig_new();
  PS_ASSERT(userconfig)

  PS_ASSERT_INTS(0,ps_userconfig_count_fields(userconfig),"newly-created userconfig")
  PS_ASSERT_FAILURE(ps_userconfig_get_field_declaration(0,0,userconfig,0),"newly-created userconfig")

  PS_ASSERT_CALL(ps_userconfig_declare_boolean(userconfig,"enable-feature",1))
  PS_ASSERT_CALL(ps_userconfig_declare_integer(userconfig,"number-of-things",42,0,100))
  PS_ASSERT_CALL(ps_userconfig_declare_string (userconfig,"name-of-thing","Henry",-1))
  PS_ASSERT_CALL(ps_userconfig_declare_path   (userconfig,"path-to-file","/etc/wherever",-1,0))

  PS_ASSERT_INTS(4,ps_userconfig_count_fields(userconfig),"after declaring fields")
  #define ASSERT_FIELD(fldp,typetag,name) { \
    int type_actual=-1,namec_actual=-1; \
    const char *name_actual=0; \
    PS_ASSERT_CALL(namec_actual=ps_userconfig_get_field_declaration(&type_actual,&name_actual,userconfig,fldp),"expected '%s'",name) \
    PS_ASSERT_INTS(namec_actual,sizeof(name)-1,"for field '%s'",name) \
    PS_ASSERT_INTS(type_actual,PS_USERCONFIG_TYPE_##typetag,"for field '%s'",name) \
    PS_ASSERT_STRINGS(name,sizeof(name)-1,name_actual,namec_actual) \
  }
  /* Not the order we declared them; they sort by key length then key content. */
  ASSERT_FIELD(0,PATH,"path-to-file")
  ASSERT_FIELD(1,STRING,"name-of-thing")
  ASSERT_FIELD(2,BOOLEAN,"enable-feature")
  ASSERT_FIELD(3,INTEGER,"number-of-things")
  #undef ASSERT_FIELD

  /* Confirm they all took their initializers. */
  PS_ASSERT_INTS(ps_userconfig_get_field_as_int(userconfig,2),1)
  PS_ASSERT_INTS(ps_userconfig_get_field_as_int(userconfig,3),42)
  const char *v; int vc;
  PS_ASSERT_CALL(vc=ps_userconfig_peek_field_as_string(&v,userconfig,0))
  PS_ASSERT_STRINGS(v,vc,"/etc/wherever",-1)
  PS_ASSERT_CALL(vc=ps_userconfig_peek_field_as_string(&v,userconfig,1))
  PS_ASSERT_STRINGS(v,vc,"Henry",-1)

  ps_userconfig_del(userconfig);
  return 0;
}

/* Test encode and decode.
 */

PS_TEST(test_userconfig_basic_encoding,userconfig) {
  struct ps_userconfig *userconfig=ps_new_userconfig_with_common_fields();
  PS_ASSERT(userconfig)

  PS_ASSERT_CALL(ps_userconfig_decode(userconfig,
    "# This is a line comment.\n"
    "enable=TRUE\n"
    "  value = 3 # comment\n"
    "\tname\x1f= a b c \n"
    "path=/abc/defg\n"
    "#path=/abc/def/ghi/jkl\n"
  ,-1))

  PS_ASSERT_INTS(ps_userconfig_get_int(userconfig,"enable",-1),1)
  PS_ASSERT_INTS(ps_userconfig_get_int(userconfig,"value",-1),3)
  PS_ASSERT_STRINGS(ps_userconfig_get_str(userconfig,"name",-1),-1,"a b c",-1)
  PS_ASSERT_STRINGS(ps_userconfig_get_str(userconfig,"path",-1),-1,"/abc/defg",-1)

  PS_ASSERT_CALL(ps_userconfig_decode(userconfig,
    "value=12\n"
    "name=54321\n"
  ,-1))

  PS_ASSERT_INTS(ps_userconfig_get_int(userconfig,"enable",-1),1)
  PS_ASSERT_INTS(ps_userconfig_get_int(userconfig,"value",-1),12)
  PS_ASSERT_STRINGS(ps_userconfig_get_str(userconfig,"name",-1),-1,"54321",-1)
  PS_ASSERT_STRINGS(ps_userconfig_get_str(userconfig,"path",-1),-1,"/abc/defg",-1)

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_encode(&buffer,userconfig))
  //ps_log(TEST,DEBUG,"Encoded userconfig:\n%.*s",buffer.c,buffer.v);
  PS_ASSERT_STRINGS(buffer.v,buffer.c,
    "name=54321\n"
    "path=/abc/defg\n"
    "value=12\n"
    "enable=1\n"
  ,-1)
  ps_buffer_cleanup(&buffer);

  /* "value" has limits of -100..100 */
  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"value",-1,"-100",-1))
  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"value",-1,"100",-1))
  PS_ASSERT_FAILURE(ps_userconfig_set(userconfig,"value",-1,"-101",-1))
  PS_ASSERT_FAILURE(ps_userconfig_set(userconfig,"value",-1,"101",-1))

  ps_userconfig_del(userconfig);
  return 0;
}

/* Test various scenarios for reencode.
 */

PS_TEST(test_userconfig_generates_default_file_on_reencode_if_input_empty,userconfig) {
  struct ps_userconfig *userconfig=ps_new_userconfig_with_common_fields();
  PS_ASSERT(userconfig)

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_reencode(&buffer,"",0,userconfig))
  PS_ASSERT_STRINGS(buffer.v,buffer.c,"name=\npath=\nvalue=0\nenable=0\n",-1)

  ps_buffer_cleanup(&buffer);
  ps_userconfig_del(userconfig);
  return 0;
}

PS_TEST(test_userconfig_preserves_whitespace_and_comments,userconfig) {
  struct ps_userconfig *userconfig=ps_new_userconfig_with_common_fields();
  PS_ASSERT(userconfig)

  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"name",4,"John Doe",-1))
  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"path",4,"/etc/wherever",-1))

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_reencode(&buffer,
    "# This is my config file\n"
    "\n"
    " path = /old/path \n"
    "\n"
    "# Line comment.\n"
    "name Old Name # First name 'Old', yeah.\n"
    "\n"
    "value :12345\n"
    "enable true\n"
  ,-1,userconfig))
  //ps_log(TEST,INFO,"Encoded:\n%.*s\n",buffer.c,buffer.v);
  PS_ASSERT_STRINGS(buffer.v,buffer.c,
    "# This is my config file\n"
    "\n"
    " path = /etc/wherever \n"
    "\n"
    "# Line comment.\n"
    "name John Doe # First name 'Old', yeah.\n"
    "\n"
    "value :0\n"
    "enable 0\n"
  ,-1)

  ps_buffer_cleanup(&buffer);
  ps_userconfig_del(userconfig);
  return 0;
}

static void compare_strings(const void *_a,int ac,const void *_b,int bc) {
  const uint8_t *a=_a;
  const uint8_t *b=_b;
  if (ac<0) { ac=0; while (a[ac]) ac++; }
  if (bc<0) { bc=0; while (b[bc]) bc++; }
  int rowlen=16;
  int p=0;
  while ((p<ac)&&(p<bc)) {
    int i;
    
    for (i=0;i<rowlen;i++) {
      int cha,chb;
      if (p+i>=ac) cha=-256; else cha=a[p+i];
      if (p+i>=bc) chb=-256; else chb=b[p+i];
      if (cha!=chb) printf("\x1b[31m");
      printf(" %02x",cha&0xff);
      if (cha!=chb) printf("\x1b[0m");
    }
    printf(" |||");
    for (i=0;i<rowlen;i++) {
      int cha,chb;
      if (p+i>=ac) cha=-256; else cha=a[p+i];
      if (p+i>=bc) chb=-256; else chb=b[p+i];
      if (cha!=chb) printf("\x1b[31m");
      printf(" %02x",chb&0xff);
      if (cha!=chb) printf("\x1b[0m");
    }
    printf("\n");
      
    p+=rowlen;
  }
}

PS_TEST(test_userconfig_reencode_preserves_equivalent_values,userconfig) {
  struct ps_userconfig *userconfig=ps_new_userconfig_with_common_fields();
  PS_ASSERT(userconfig)

  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"name",4,"John Doe",-1))

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_reencode(&buffer,
    "enable:false\n"
    "name =   \r\n"
    " path:\t\n\r"
    "value -00000\n"
  ,-1,userconfig))
  //ps_log_hexdump(TEST,DEBUG,buffer.v,buffer.c,"Encoded");
  PS_ASSERT_STRINGS(buffer.v,buffer.c,
    "enable:false\n"
    "name =   John Doe\r\n"
    " path:\t\n\r"
    "value -00000\n"
  ,-1)

  ps_buffer_cleanup(&buffer);
  ps_userconfig_del(userconfig);
  return 0;
}

PS_TEST(test_userconfig_reencode_uncomments_line_if_value_matches,userconfig) {
  struct ps_userconfig *userconfig=ps_new_userconfig_with_common_fields();
  PS_ASSERT(userconfig)

  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"name",4,"John Doe",-1))

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_reencode(&buffer,
    "#name=Jane Doe\n"
    " # name = Buck Doe\n"
    " # name = John Doe\n"
    "name=Bambi Doe\n"
    "path=\n"
    "value=0\n"
    "#enable false # Comment\n"
    "enable=true\n"
  ,-1,userconfig))
  //ps_log(TEST,INFO,"Encoded:\n%.*s\n",buffer.c,buffer.v);
  PS_ASSERT_STRINGS(buffer.v,buffer.c,
    "#name=Jane Doe\n"
    " # name = Buck Doe\n"
    "name = John Doe\n"
    "# name=Bambi Doe\n"
    "path=\n"
    "value=0\n"
    "enable false # Comment\n"
    "# enable=true\n"
  ,-1)

  ps_buffer_cleanup(&buffer);
  ps_userconfig_del(userconfig);
  return 0;
}

PS_TEST(test_userconfig_reencode_comments_line_if_invalid,userconfig) {
  struct ps_userconfig *userconfig=ps_new_userconfig_with_common_fields();
  PS_ASSERT(userconfig)

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_reencode(&buffer,
    "moon=MOON\n"
    "name=\n"
    "name=another\n"
    "enable=0\n"
    "enable=falsetto\n"
    "path=\n"
    "nosuch=\n"
    "value:not a number # Invalid value counts as 'not equal'\n"
  ,-1,userconfig))
  //ps_log(TEST,INFO,"Encoded:\n%.*s\n",buffer.c,buffer.v);
  PS_ASSERT_STRINGS(buffer.v,buffer.c,
    "# moon=MOON\n"
    "name=\n"
    "# name=another\n"
    "enable=0\n"
    "# enable=falsetto\n"
    "path=\n"
    "# nosuch=\n"
    "value:0 # Invalid value counts as 'not equal'\n"
  ,-1)

  ps_buffer_cleanup(&buffer);
  ps_userconfig_del(userconfig);
  return 0;
}

/* Defect observed on Raspberry Pi:
 * On the first launch, when "resources" and "input" are defaulted, if we change "sound", it gets assigned to "soft-render".
 *
 * OK, reproduced the defect. Please don't modify this test.
 * Before any repairs, it encodes this:
#resources=
resources=1
#input=
input=/path/to/input
fullscreen=false199
soft-render=198
music=255
sound=255
 */

PS_TEST(test_defect_userconfig_assigns_sound_to_soft_render,userconfig) {
  struct ps_userconfig *userconfig=ps_userconfig_new();
  PS_ASSERT(userconfig)
  PS_ASSERT_CALL(ps_userconfig_declare_default_fields(userconfig))

  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"resources",-1,"src/data",-1))
  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"input",-1,"etc/input.cfg",-1))
  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"music",-1,"199",-1))
  PS_ASSERT_CALL(ps_userconfig_set(userconfig,"sound",-1,"198",-1))

  PS_ASSERT_CALL(ps_userconfig_commit_paths(userconfig))

  const char *tmp;
  int tmpc;
  PS_ASSERT_CALL(tmpc=ps_userconfig_peek_field_as_string(&tmp,userconfig,ps_userconfig_search_field(userconfig,"resources",-1)))
  PS_ASSERT_STRINGS(tmp,tmpc,"src/data",-1)
  PS_ASSERT_CALL(tmpc=ps_userconfig_peek_field_as_string(&tmp,userconfig,ps_userconfig_search_field(userconfig,"input",-1)))
  PS_ASSERT_STRINGS(tmp,tmpc,"etc/input.cfg",-1)
  PS_ASSERT_INTS(1,ps_userconfig_get_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"fullscreen",-1)))
  PS_ASSERT_INTS(0,ps_userconfig_get_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"soft-render",-1)))
  PS_ASSERT_INTS(199,ps_userconfig_get_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"music",-1)))
  PS_ASSERT_INTS(198,ps_userconfig_get_field_as_int(userconfig,ps_userconfig_search_field(userconfig,"sound",-1)))

  struct ps_buffer buffer={0};
  PS_ASSERT_CALL(ps_userconfig_reencode(&buffer,
    "#resources=\n"
    "#input=\n"
    "fullscreen=false\n"
    "soft-render=false\n"
    "music=255\n"
    "sound=255\n"
  ,-1,userconfig))

  PS_ASSERT_STRINGS(buffer.v,buffer.c,
    "#resources=\n"
    "resources=src/data\n"
    "#input=\n"
    "input=etc/input.cfg\n"
    "fullscreen=1\n"
    "soft-render=false\n"
    "music=199\n"
    "sound=198\n"
    "highscores=\n"
  ,-1)

  ps_buffer_cleanup(&buffer);
  ps_userconfig_del(userconfig);
  return 0;
}
