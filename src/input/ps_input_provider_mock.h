/* ps_input_provider_mock.h
 * An input provider that reports dummy devices, for testing.
 */
 
#ifndef PS_INPUT_PROVIDER_MOCK_H
#define PS_INPUT_PROVIDER_MOCK_H

struct ps_input_provider *ps_input_provider_mock_new();

/* To keep it simple for consumers, you can refer to mock devices by name.
 * Very inefficient, but this is only for testing.
 * Mock devices have the 16 player buttons.
 */
int ps_input_provider_mock_add_device(struct ps_input_provider *provider,const char *name);
int ps_input_provider_mock_remove_device(struct ps_input_provider *provider,const char *name);
int ps_input_provider_mock_set_button(struct ps_input_provider *provider,const char *name,int btnid,int value);

#endif
