#include "test/ps_test.h"
#include "input/ps_input_report_reader.h"

/* Basic setup and lifecycle assertions.
 */

PS_TEST(test_input_report_reader_lifecycle,input,rptrd) {
  struct ps_input_report_reader *reader=ps_input_report_reader_new();
  PS_ASSERT(reader)
  PS_ASSERT_NOT(ps_input_report_reader_is_ready(reader))

  /* Byte order can be '<' or '>', nothing else. */
  PS_ASSERT_CALL(ps_input_report_reader_set_byte_order(reader,'>'))
  PS_ASSERT_CALL(ps_input_report_reader_set_byte_order(reader,'<'))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,0))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,' '))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,'l'))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,'L'))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,'b'))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,'B'))
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,0xff))
  PS_ASSERT_NOT(ps_input_report_reader_is_ready(reader))

  /* We are not finalized, so we must reject any reports. */
  PS_ASSERT_FAILURE(ps_input_report_reader_set_report(reader,"",0,0,0),"Before finalizing.")

  /* Finalize. */
  PS_ASSERT_CALL(ps_input_report_reader_finish_setup(reader))
  PS_ASSERT(ps_input_report_reader_is_ready(reader))
  PS_ASSERT_INTS(0,ps_input_report_reader_report_length(reader))
  PS_ASSERT_INTS(0,ps_input_report_reader_count_fields(reader))

  /* Must reject all further setup. */
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,'<'),"After finalizing.")
  PS_ASSERT_FAILURE(ps_input_report_reader_set_byte_order(reader,'>'),"After finalizing.")
  PS_ASSERT_FAILURE(ps_input_report_reader_add_padding(reader,8),"After finalizing.")
  PS_ASSERT_FAILURE(ps_input_report_reader_pad_to_multiple(reader,8),"After finalizing.")
  PS_ASSERT_FAILURE(ps_input_report_reader_add_field(reader,8,0,1),"After finalizing.")
  PS_ASSERT_FAILURE(ps_input_report_reader_add_field_at(reader,0,8,0,1),"After finalizing.")
  PS_ASSERT_FAILURE(ps_input_report_reader_finish_setup(reader),"Already finished.")

  /* Must accept reports now. */
  PS_ASSERT_CALL(ps_input_report_reader_set_report(reader,"",0,0,0),"After finalizing.")

  ps_input_report_reader_del(reader);
  return 0;
}

/* Helper for catching report callbacks.
 */

#define PS_TEST_EVENT_LOG_LIMIT 256

struct ps_test_event_log {
  struct {
    int fldix;
    int fldid;
    int value;
    int prev;
  } eventv[PS_TEST_EVENT_LOG_LIMIT];
  int eventp,eventc;
};

static void ps_test_event_log_clear(struct ps_test_event_log *log) {
  log->eventp=0;
  log->eventc=0;
}

static int ps_test_event_log_record(struct ps_test_event_log *log,int fldix,int fldid,int value,int prev) {
  PS_ASSERT_INTS_OP(log->eventc,<,PS_TEST_EVENT_LOG_LIMIT)
  int p=(log->eventp+log->eventc)%PS_TEST_EVENT_LOG_LIMIT;
  log->eventv[p].fldix=fldix;
  log->eventv[p].fldid=fldid;
  log->eventv[p].value=value;
  log->eventv[p].prev=prev;
  log->eventc++;
  return 0;
}

static int ps_test_event_log_pop_and_assert(struct ps_test_event_log *log,int fldix,int fldid,int value,int prev) {
  PS_ASSERT_INTS_OP(log->eventc,>,0)
  PS_ASSERT_INTS(log->eventv[log->eventp].fldix,fldix,"(%d,%d,%d,%d)",fldix,fldid,value,prev)
  PS_ASSERT_INTS(log->eventv[log->eventp].fldid,fldid,"(%d,%d,%d,%d)",fldix,fldid,value,prev)
  PS_ASSERT_INTS(log->eventv[log->eventp].value,value,"(%d,%d,%d,%d)",fldix,fldid,value,prev)
  PS_ASSERT_INTS(log->eventv[log->eventp].prev,prev,"(%d,%d,%d,%d)",fldix,fldid,value,prev)
  if (++(log->eventp)>=PS_TEST_EVENT_LOG_LIMIT) log->eventp=0;
  log->eventc--;
  return 0;
}

/* Test all field arrangements, and the callback mechanism.
 */

static int test_input_report_reader_operations_setup(struct ps_input_report_reader *reader) {

  /* [0]: bits #1..6. Leave a hole. Use bits 0xe7, and leave 0x18 unused. */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,1,0,1))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,1,0,2))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,1,0,3))
  PS_ASSERT_CALL(ps_input_report_reader_add_padding(reader,2))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,1,0,4))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,1,0,5))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,1,0,6))

  /* [1]: Small oddball field #7. 5 signed bits in 0x3e. 0xc1 unused. */
  PS_ASSERT_CALL(ps_input_report_reader_add_padding(reader,1))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,5,1,7))
  PS_ASSERT_CALL(ps_input_report_reader_pad_to_multiple(reader,8))

  /* [2..3]: Large oddball field #8. 10 unsigned bits in [0xf0,0x3f] or [0x0f,0xfc]. */
  PS_ASSERT_CALL(ps_input_report_reader_add_padding(reader,4))
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,10,0,8))
  PS_ASSERT_CALL(ps_input_report_reader_pad_to_multiple(reader,8))

  /* [4..5]: U16 #9 */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,16,0,9))

  /* [6..7]: S16 #10 */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,16,1,10))

  /* [8..11]: U32 #11 */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,32,0,11))

  /* [12..15]: S32 #12 */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,32,1,12))

  /* [16]: U8 #13 */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,8,0,13))

  /* [17]: S8 #14 */
  PS_ASSERT_CALL(ps_input_report_reader_add_field(reader,8,1,14))

  PS_ASSERT_CALL(ps_input_report_reader_finish_setup(reader))
  PS_ASSERT_INTS(18,ps_input_report_reader_report_length(reader))
  return 0;
}

static int cb_input_report_reader_operations_report(struct ps_input_report_reader *reader,int fldix,int fldid,int value,int prev,void *userdata) {
  struct ps_test_event_log *log=userdata;
  if (ps_test_event_log_record(log,fldix,fldid,value,prev)<0) return -1;
  return 0;
}

static int test_input_report_reader_operations_report(struct ps_input_report_reader *reader,struct ps_test_event_log *log,const void *report) {
  ps_test_event_log_clear(log);
  PS_ASSERT_CALL(ps_input_report_reader_set_report(reader,report,18,log,cb_input_report_reader_operations_report))
  return 0;
}

PS_TEST(test_input_report_reader_operations_be,input,rptrd) {
  struct ps_test_event_log log;
  struct ps_input_report_reader *reader=ps_input_report_reader_new();
  PS_ASSERT(reader)
  PS_ASSERT_CALL(ps_input_report_reader_set_byte_order(reader,'>'))
  if (test_input_report_reader_operations_setup(reader)<0) return -1;

  if (test_input_report_reader_operations_report(
    reader,&log,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  )<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)

  if (test_input_report_reader_operations_report(
    reader,&log,"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
  )<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,5,6,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,4,5,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,3,4,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,2,3,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,1,2,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,0,1,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,6,7,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,7,8,1023,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,8,9,65535,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,9,10,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,10,11,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,11,12,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,12,13,255,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,13,14,-1,0)<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)

  if (test_input_report_reader_operations_report(
    reader,&log,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  )<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,5,6,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,4,5,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,3,4,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,2,3,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,1,2,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,0,1,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,6,7,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,7,8,0,1023)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,8,9,0,65535)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,9,10,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,10,11,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,11,12,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,12,13,0,255)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,13,14,0,-1)<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)

  if (test_input_report_reader_operations_report(
    reader,&log,"\0\x3c\x04\x04\x02\x03\x80\0\x80\0\0\0\x11\x22\x33\x44\x80\x80"
  )<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,6,7,15,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,7,8,257,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,8,9,515,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,9,10,-32768,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,10,11,INT_MIN,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,11,12,0x11223344,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,12,13,128,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,13,14,-128,0)<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)
  
  ps_input_report_reader_del(reader);
  return 0;
}

PS_TEST(test_input_report_reader_operations_le,input,rptrd) {
  struct ps_test_event_log log;
  struct ps_input_report_reader *reader=ps_input_report_reader_new();
  PS_ASSERT(reader)
  PS_ASSERT_CALL(ps_input_report_reader_set_byte_order(reader,'<'))
  if (test_input_report_reader_operations_setup(reader)<0) return -1;

  if (test_input_report_reader_operations_report(
    reader,&log,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  )<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)

  if (test_input_report_reader_operations_report(
    reader,&log,"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
  )<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,0,1,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,1,2,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,2,3,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,3,4,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,4,5,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,5,6,1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,6,7,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,7,8,1023,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,8,9,65535,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,9,10,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,10,11,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,11,12,-1,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,12,13,255,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,13,14,-1,0)<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)

  if (test_input_report_reader_operations_report(
    reader,&log,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  )<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,0,1,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,1,2,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,2,3,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,3,4,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,4,5,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,5,6,0,1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,6,7,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,7,8,0,1023)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,8,9,0,65535)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,9,10,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,10,11,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,11,12,0,-1)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,12,13,0,255)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,13,14,0,-1)<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)

  if (test_input_report_reader_operations_report(
    reader,&log,"\0\x1e\x10\x10\x03\x02\0\x80\0\0\0\x80\x11\x22\x33\x44\x80\x80"
  )<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,6,7,15,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,7,8,257,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,8,9,515,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,9,10,-32768,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,10,11,INT_MIN,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,11,12,0x44332211,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,12,13,128,0)<0) return -1;
  if (ps_test_event_log_pop_and_assert(&log,13,14,-128,0)<0) return -1;
  PS_ASSERT_INTS(log.eventc,0)
  
  ps_input_report_reader_del(reader);
  return 0;
}
