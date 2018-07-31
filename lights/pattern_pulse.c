/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <pthread.h>
#include <assert.h>

#include "rpi_ws281x/clk.h"
#include "rpi_ws281x/gpio.h"
#include "rpi_ws281x/dma.h"
#include "rpi_ws281x/pwm.h"
#include "version.h"

#include "rpi_ws281x/ws2811.h"
#include "pattern_pulse.h"
#include "log.h"

static ws2811_led_t color = 0;
static bool newColor = false;
static uint32_t intensity = 0;

/* A new color has been injected. What happens to old color? */
ws2811_return_t
pulse_inject(ws2811_led_t in_color, uint32_t in_intensity)
{
    log_trace("pulse_inject(): %d, %d\n", in_color, in_intensity);
    ws2811_return_t ret = WS2811_SUCCESS;
    color = in_color;
    newColor = true;
    intensity = in_intensity;
    return ret;
}

/* Run the threaded loop */
void *
matrix_run2(void *vargp)
{
    log_matrix_trace("pulse_run()");

    ws2811_return_t ret = WS2811_SUCCESS;
    struct pattern *pattern = (struct pattern*)vargp;
    assert(pattern->running);
    int i = 0;
    bool rampUp = true;
    bool colorFinished = true;
    newColor = false;
    double prev_amp = 0;
    uint32_t r_shift;
    uint32_t g_shift;
    uint32_t b_shift;
    uint32_t red, green, blue;
    double scalar;
    double slope;
    uint32_t pulseWidth;
    uint32_t maxStripBrightness;
    while (pattern->running)
    {
        if (!pattern->paused) {
            move_lights(pattern, 1);
            /* Set up boundary conditions of a single pulse */
            if (newColor) {
                // get prev_amp of previous pattern, which if the other ended, it should be zero
                r_shift = (color & 0xFF0000) >> 16;
                g_shift = (color & 0x00FF00) >> 8;
                b_shift = (color & 0x0000FF);
                colorFinished = true;

                log_matrix_trace("Injecting");
                //prev_amp = 0;
                i = 0;
                newColor = false;
                rampUp = true;
                colorFinished = false;
                
                if (pattern->pulseWidth == 0) {
                    pulseWidth = intensity / 5;
                }
                else {
                    pulseWidth = pattern->pulseWidth;
                }
                
                if (pulseWidth <= 1) {
		            pulseWidth = 2;
		        }
                /* Calculate the slope at wherever the CURRENT led's brightness is
                 * to the maximum brightness. This could be interupting a different pulse,
                 * we ramp up from where we are */
                /* Start with the maximum desired brightness */
                // XXX rename me
                maxStripBrightness = pattern->ledstring->channel[0].brightness;
                /* Divide maximum brightness by the width of the pulse */
                slope = (double)(((double)(maxStripBrightness-(prev_amp*256))/(double)pulseWidth) / 256);
                /* Multiply by the intensity */
                slope = slope * (double)((double)intensity/(double)100);
                //printf("Slope: %lf Prev_Amp: %lf Total: %lf Intensity: %lf \n", slope, prev_amp,
                //            ((slope*pulseWidth)+prev_amp), (double)((double)maxStripBrightness/(double)256));
            }
            /* Pulse is finished, insert a blank */
            else if (colorFinished) {
                pattern->ledstring->channel[0].leds[0] = 0;
            } 
            //printf("Prev Amp: %lf\n", prev_amp);
            if (!colorFinished) { 
                if (rampUp && ((uint32_t)i == ((uint32_t)pulseWidth-1))) {
                    rampUp = false;
                    maxStripBrightness = pattern->ledstring->channel[0].brightness;
                    /* Recalculate slope for coming down to 0*/
                    slope = (double)(((double)maxStripBrightness/(double)pulseWidth) / 256);
                    slope = slope * (double)((double)intensity/(double)100);
                }
                
                scalar = prev_amp;
                prev_amp = (rampUp) ? (prev_amp + slope) : (prev_amp - slope);

                red = ((uint32_t)(r_shift * scalar) << 16);
                green = ((uint32_t)(g_shift * scalar) << 8);
                blue = ((uint32_t)(b_shift * scalar));

                pattern->ledstring->channel[0].leds[0] = (red+green+blue);
                log_matrix_trace("Injecting %d %d %d\n", i, red, green, blue);
                i = (rampUp) ? (i + 1) : (i - 1);
                
                if (i == 0) {
                    colorFinished = true;
                    prev_amp = 0;
                }
                assert(i >= 0);
            }

            if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
                log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                // xxx: this should cause some sort of fatal error to propogate upwards
                break;
            }
        }
        usleep(1000000 / pattern->movement_rate);
    }
    return NULL;
}

/* Initialize everything, and begin the thread */
ws2811_return_t
pulse_load(struct pattern *pattern)
{
    log_trace("pulse_load()");

    /* A protection against matrix_run() being called in a bad order. */
    pattern->running = 1;

    pthread_create(&pattern->thread_id, NULL, matrix_run2, pattern);
    log_info("Pattern Pulse: Loop is now running.");
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_start(struct pattern *pattern)
{
    log_trace("pulse_start()");
    pattern->paused = false;
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_stop(struct pattern *pattern)
{
    log_trace("pulse_stop()");
    pattern->paused = true;
    //matrix_clear(pattern);
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_clear(struct pattern *pattern)
{
    log_trace("pulse_clear()\n");
    log_info("Pattern Pulse: Clearing Pattern");
    ws2811_return_t ret = WS2811_SUCCESS;
    uint32_t i;
    uint32_t led_count = pattern->ledstring->channel[0].count;
    for (i = 0; i < led_count; i++) {
        pattern->ledstring->channel[0].leds[i] = 0;
    }

    if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS) {
        log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
        // xxx: this should cause some sort of fatal error to propogate upwards
    }
    return ret;
}

ws2811_return_t
pulse_pause(struct pattern *pattern)
{
    log_trace("pulse_pause()");
    pattern->paused = true;
    return WS2811_SUCCESS;
}

/* Turn off strip, kill second process */
ws2811_return_t
pulse_kill(struct pattern *pattern)
{
    log_trace("pulse_kill()");
    log_debug("Pattern Pulse: Stopping run");
    pattern->running = 0;

    log_debug("Pattern Pulse: Loop Waiting for thread %d to end", pattern->thread_id);
    pthread_join(pattern->thread_id, NULL);

    if (pattern->clear_on_exit) {
        pulse_clear(pattern);
    }

    log_info("Pattern Pulse: Loop now stopped");
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_create(struct pattern *pattern)
{
    log_trace("pulse_create()");
    /* Assign function pointers */
    pattern->func_load_pattern = &pulse_load;
    pattern->func_start_pattern = &pulse_start;
    pattern->func_kill_pattern = &pulse_kill;
    pattern->func_pause_pattern = &pulse_pause;
    pattern->func_inject = &pulse_inject;

    /* Set default values */
    pattern->running = true;
    pattern->paused = true;
    pattern->pulseWidth = 0;
    return WS2811_SUCCESS;
}   

ws2811_return_t
pulse_delete(struct pattern *pattern)
{
    log_trace("pulse_delete()");
    log_debug("Pattern Pulse: Freeing objects");
    assert(pattern != NULL);
    return WS2811_SUCCESS;
}
