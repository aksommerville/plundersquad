#ifndef PS_MSHID_INTERNAL_H
#define PS_MSHID_INTERNAL_H

#include "ps.h"
#include "ps_mshid.h"
#include "input/ps_input.h"
#include "input/ps_input_provider.h"
#include "input/ps_input_device.h"
#include "input/ps_input_report_reader.h"
#include "input/ps_input_button.h"
#include <windows.h>
#include <ddk/hidsdi.h>

extern struct ps_input_provider *ps_mshid_provider;

#endif
