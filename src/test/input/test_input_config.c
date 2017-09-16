#include "test/ps_test.h"
#include "input/ps_input_internal.h"

/* Validate simple assumptions, creating object and populating members.
 */

PS_TEST(test_input_config_object,input) {

  struct ps_input_config *config=ps_input_config_new();
  PS_ASSERT(config,"ps_input_config_new()")

  /* Assert default object state. */
  PS_ASSERT_NOT(config->path)

  /* Test member accessors. */
  PS_ASSERT_CALL(ps_input_config_set_path(config,"/path/to/config/file",-1))
  PS_ASSERT_STRINGS(ps_input_config_get_path(config),-1,"/path/to/config/file",-1)
  PS_ASSERT_CALL(ps_input_config_set_path(config,"",0))
  PS_ASSERT_NOT(config->path,"Setting path empty should null it out.")

  ps_input_config_del(config);
  return 0;
}
