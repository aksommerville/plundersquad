#include "ps_test.h"
#include "ps_test_contents.h"

// This scaffold fails to compile if there isn't at least one test.
PS_TEST(dummy_test) {
  return 0;
}

/* Globals.
 */

int ps_test_verbosity=0;
int ps_test_color=1;
static int ps_test_use_exit_status=1;

static int ps_test_resultv[ps_testc]={0};

/* Receive short options. There are no long options.
 */

static int ps_test_rcvarg(const char *src) {
  for (;*src;src++) switch (*src) {
    case 'v': ps_test_verbosity++; break;
    case 'q': ps_test_verbosity--; break;
    case 't': ps_test_color=0; break;
    case 'e': ps_test_use_exit_status=0; break;
    default: {
        fprintf(stderr,"Unexpected short option '%c'.\n",*src);
        return -1;
      }
  }
  return 0;
}

/* Test string for loggability.
 */

int ps_string_loggable(const char *src,int srcc) {
  if (srcc<0) return 0;
  if (!srcc) return 1;
  if (!src) return 0;
  if (srcc>100) return 0;
  for (;srcc-->0;src++) {
    if (*src<0x20) return 0;
    if (*src>0x7e) return 0;
  }
  return 1;
}

/* Run a test. Returns -1 or 1, nothing else.
 */

static int ps_test_run(const struct ps_test *test) {
  if (ps_test_verbosity>=PS_TEST_VERBOSITY_PRELAUNCH) {
    printf("Running test '%s' [%s:%d]...\n",test->name,test->file,test->line);
  }
  int result=test->fn();
  if (result<0) return -1;
  return 1;
}

/* Locate string in comma-delimited list.
 */

static int ps_test_in_group(const char *groups,const char *arg) {
  if (!groups||!arg) return 0;
  int argc=0; while (arg[argc]) argc++;
  while (*groups) {
    if (*groups==',') { groups++; continue; }
    if ((unsigned char)*groups<=0x20) { groups++; continue; }
    const char *ck=groups;
    int ckc=0;
    while (*groups&&(*groups!=',')) { groups++; ckc++; }
    while (ckc&&((unsigned char)ck[ckc-1]<=0x20)) ckc--;
    if (ckc!=argc) continue;
    if (memcmp(ck,arg,argc)) continue;
    return 1;
  }
  return 0;
}

/* Run all tests matching a request string which haven't been run yet.
 */

static void ps_test_run_named(const char *arg) {
  const struct ps_test *test=ps_testv;
  int i=0; for (;i<ps_testc;i++,test++) {
    if (ps_test_resultv[i]) continue;
    if (!strcmp(arg,test->name)||ps_test_in_group(test->groups,arg)) {
      ps_test_resultv[i]=ps_test_run(test);
    }
  }
}

/* Run all tests which haven't run yet and are not in the "ignore" group.
 */

static void ps_test_run_unignored() {
  const struct ps_test *test=ps_testv;
  int i=0; for (;i<ps_testc;i++,test++) {
    if (ps_test_resultv[i]) continue;
    if (ps_test_in_group(test->groups,"ignore")) continue;
    ps_test_resultv[i]=ps_test_run(test);
  }
}

/* Print a list of test names which match the given result.
 */

static void ps_test_list_for_result(const char *description,int result,int count) {
  printf("%d tests %s:\n",count,description);
  const struct ps_test *test=ps_testv;
  int i; for (i=0;i<ps_testc;i++,test++) {
    if (ps_test_resultv[i]!=result) continue;
    printf("  %s [%s:%d]\n",test->name,test->file,test->line);
  }
}

/* Print final report and return exit status.
 */

static int ps_test_report_results() {

  /* Gather counts by result. */
  int passc=0,failc=0,skipc=0,i;
  for (i=0;i<ps_testc;i++) {
    if (ps_test_resultv[i]<0) failc++;
    else if (ps_test_resultv[i]>0) passc++;
    else skipc++;
  }

  /* Print the final one-line report. */
  if (ps_test_verbosity>=PS_TEST_VERBOSITY_FINAL_REPORT) {
    const char *flag;
    if (ps_test_color) {
           if (failc) flag="\x1b[41m    \x1b[0m";
      else if (passc) flag="\x1b[42m    \x1b[0m";
      else            flag="\x1b[40m    \x1b[0m";
    } else {
           if (failc) flag="[FAIL]";
      else if (passc) flag="[PASS]";
      else            flag="[----]";
    }
    printf("%s %d fail, %d pass, %d skip.\n",flag,failc,passc,skipc);
  }

  /* List failed and skipped tests. */
  if (failc&&(ps_test_verbosity>=PS_TEST_VERBOSITY_LIST_FAILURES)) {
    ps_test_list_for_result("failed",-1,failc);
  }
  if (skipc&&(ps_test_verbosity>=PS_TEST_VERBOSITY_LIST_SKIPS)) {
    ps_test_list_for_result("skipped",0,skipc);
  }

  /* Return a failing exit status if requested, and if at least one failed. */
  if (ps_test_use_exit_status&&failc) return 1;
  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {

  int run_default=1;
  int argi=1; for (;argi<argc;argi++) {
    const char *arg=argv[argi];
    if (arg[0]=='-') {
      if (ps_test_rcvarg(arg+1)<0) {
        return 1;
      }
    } else {
      ps_test_run_named(arg);
      run_default=0;
    }
  }

  if (run_default) {
    ps_test_run_unignored();
  }

  return ps_test_report_results();
}

/* Stubs.
 */

int ps_main_input_action_callback(int actionid) {
  return 0;
}
