#ifndef __PATTERN_PULSE_H
#define __PATTERN_PULSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rpi_ws281x/ws2811.h"
#include "pattern.h"
ws2811_return_t pulse_create(struct pattern *pattern);
ws2811_return_t pulse_delete(struct pattern*);
#ifdef __cplusplus
}
#endif

#endif /* __PATTERN_PULSE_H */
