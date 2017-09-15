#ifndef PS_TEST_H
#define PS_TEST_H

#include "ps.h"

#define PS_TEST(name,...) int name()
#define XXX_PS_TEST(name,...) int name()

extern int ps_test_verbosity;
extern int ps_test_color;

int ps_string_loggable(const char *src,int srcc);

/* Minimum verbosity level for various events. */
#define PS_TEST_VERBOSITY_LOG_NORMAL       0
#define PS_TEST_VERBOSITY_LOG_DEBUG        1
#define PS_TEST_VERBOSITY_FAILURE_REPORT  -1
#define PS_TEST_VERBOSITY_FINAL_REPORT    -3
#define PS_TEST_VERBOSITY_LIST_FAILURES   -2
#define PS_TEST_VERBOSITY_LIST_SKIPS      -1
#define PS_TEST_VERBOSITY_PRELAUNCH        2

/* Logging.
 *****************************************************************************/

#define PS_LOG(fmt,...) if (ps_test_verbosity>=0) { \
  ps_log(TEST,INFO,fmt,##__VA_ARGS__); \
  /*fprintf(stderr,"%s:%d: "fmt"\n",__FILE__,__LINE__,##__VA_ARGS__);*/ \
}
#define PS_DEBUG(fmt,...) if (ps_test_verbosity>=2) { \
  ps_log(TEST,DEBUG,fmt,##__VA_ARGS__); \
  /*fprintf(stderr,"%s:%d: "fmt"\n",__FILE__,__LINE__,##__VA_ARGS__);*/ \
}

#define PS_FAIL_MORE(key,fmt,...) if (ps_test_verbosity>=PS_TEST_VERBOSITY_FAILURE_REPORT) { \
  fprintf(stderr,"| %20s: "fmt"\n",key,##__VA_ARGS__) ; \
}

#define PS_FAIL_BEGIN(type,msgfmt,...) if (ps_test_verbosity>=PS_TEST_VERBOSITY_FAILURE_REPORT) { \
  fprintf(stderr,"+--------------------------------------------\n"); \
  fprintf(stderr,"| TEST FAILURE\n"); \
  PS_FAIL_MORE("Location","%s:%d",__FILE__,__LINE__) \
  if (type&&type[0]) PS_FAIL_MORE("Assertion type","%s",type) \
  if (msgfmt&&msgfmt[0]) PS_FAIL_MORE("Message",msgfmt,##__VA_ARGS__) \
}

#define PS_FAIL_END { \
  if (ps_test_verbosity>=PS_TEST_VERBOSITY_FAILURE_REPORT) { \
    fprintf(stderr,"+--------------------------------------------\n"); \
  } \
  return -1; \
}

/* Assertions.
 *****************************************************************************/

#define PS_FAIL(...) { \
  PS_FAIL_BEGIN("Forced failure",""__VA_ARGS__) \
  PS_FAIL_END \
}

#define PS_ASSERT(condition,...) if (!(condition)) { \
  PS_FAIL_BEGIN("Boolean (expected true)",""__VA_ARGS__) \
  PS_FAIL_MORE("As written","%s",#condition) \
  PS_FAIL_END \
}

#define PS_ASSERT_NOT(condition,...) if (condition) { \
  PS_FAIL_BEGIN("Boolean (expected false)",""__VA_ARGS__) \
  PS_FAIL_MORE("As written","%s",#condition) \
  PS_FAIL_END \
}

#define PS_ASSERT_INTS_OP(a,op,b,...) { \
  int _a=(a),_b=(b); \
  if (!(_a op _b)) { \
    PS_FAIL_BEGIN("Integers",""__VA_ARGS__) \
    PS_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    PS_FAIL_MORE("Values","%d %s %d",_a,#op,_b) \
    PS_FAIL_END \
  } \
}

#define PS_ASSERT_INTS(a,b,...) PS_ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define PS_ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(a),*_b=(b); \
  int _ac=(ac),_bc=(bc); \
  if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    PS_FAIL_BEGIN("Strings",""__VA_ARGS__) \
    PS_FAIL_MORE("(A) As written","%s : %s",#a,#ac) \
    PS_FAIL_MORE("(B) As written","%s : %s",#b,#bc) \
    if (ps_string_loggable(_a,_ac)) PS_FAIL_MORE("(A) Value","%.*s",_ac,_a) \
    if (ps_string_loggable(_b,_bc)) PS_FAIL_MORE("(B) Value","%.*s",_bc,_b) \
    PS_FAIL_END \
  } \
}

#define PS_ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    PS_FAIL_BEGIN("Call (expected success)",""__VA_ARGS__) \
    PS_FAIL_MORE("As written","%s",#call) \
    PS_FAIL_MORE("Result","%d",_result) \
    PS_FAIL_END \
  } \
}

#define PS_ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    PS_FAIL_BEGIN("Call (expected failure)",""__VA_ARGS__) \
    PS_FAIL_MORE("As written","%s",#call) \
    PS_FAIL_MORE("Result","%d",_result) \
    PS_FAIL_END \
  } \
}

#endif
