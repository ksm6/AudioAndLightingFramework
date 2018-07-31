#ifndef __PATTERN_STATIC_COLOR_H
#define __PATTERN_STATIC_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rpi_ws281x/ws2811.h"
#include "pattern.h"
ws2811_return_t static_color_create(struct pattern *pattern);
ws2811_return_t static_color_delete(struct pattern *pattern);
#ifdef __cplusplus
}
#endif

#endif /* __PATTERN_STATIC_COLOR_H */
