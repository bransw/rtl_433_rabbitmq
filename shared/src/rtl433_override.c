/** @file
 * Function override mechanism for rtl_433 client enhancements
 */

#include "rtl433_override.h"
#include <stdio.h>

// Global function pointer for pulse_data_print_data override
pulse_data_print_func_t g_pulse_data_print_override = NULL;

void rtl433_set_pulse_data_print_override(pulse_data_print_func_t func)
{
    g_pulse_data_print_override = func;
    if (func) {
        fprintf(stderr, "🔧 RTL433: Enhanced pulse data printing enabled\n");
    } else {
        fprintf(stderr, "🔧 RTL433: Standard pulse data printing restored\n");
    }
}

pulse_data_print_func_t rtl433_get_pulse_data_print_override(void)
{
    return g_pulse_data_print_override;
}

