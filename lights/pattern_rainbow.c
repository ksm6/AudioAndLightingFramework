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
#include "pattern_rainbow.h"
#include "log.h"

#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))
extern void move_lights(struct pattern *pattern, uint32_t shift_distance);

static uint16_t dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static ws2811_led_t dotcolors[] =
{
    0x00200000,  // red
    0x00201000,  // orange
    0x00202000,  // yellow
    0x00002000,  // green
    0x00002020,  // lightblue
    0x00000020,  // blue
    0x00100010,  // purple
    0x00200010,  // pink
};

ws2811_led_t dotcolors_rgbw[] =
{
    0x00200000,  // red
    0x10200000,  // red + W
    0x00002000,  // green
    0x10002000,  // green + W
    0x00000020,  // blue
    0x10000020,  // blue + W
    0x00101010,  // white
    0x10101010,  // white + W

};


/* Stamp onto the string */
void matrix_render(struct pattern *pattern)
{
    log_matrix_trace("matrix_renderer()");
    uint32_t x;
    uint32_t led_count = pattern->ledstring->channel[0].count;
    for (x = 0; x < led_count; x++)
    {
        pattern->ledstring->channel[0].leds[x] = pattern->matrix[x];
    }
}

void matrix_clear(struct pattern *pattern)
{
    log_matrix_trace("matrix_clear()");
    uint32_t x;
    uint32_t led_count = pattern->ledstring->channel[0].count;
    for (x = 0; x < led_count; x++) {
        pattern->matrix[x] = 0;
    }
}

void matrix_bottom(struct pattern *pattern)
{
    log_matrix_trace("matrix_bottom()");

    int i;
    uint32_t led_count = pattern->ledstring->channel[0].count;
    for (i = 0; i < (int)(ARRAY_SIZE(dotspos)); i++) {
        dotspos[i]++;
        /* Loop back to beginning of string */
        if (dotspos[i] > (led_count - 1))
        {
            dotspos[i] = 0;
        }

        /* Not mine */
        if (pattern->ledstring->channel[0].strip_type == SK6812_STRIP_RGBW) {
            pattern->matrix[dotspos[i]] = dotcolors_rgbw[i];
        }
        /* Mine */
        else {
            pattern->matrix[dotspos[i]] = dotcolors[i];
        }
    }

    /* XXX: This clears all lights that are not currently in the array */
    i = 0;
    while (i < dotspos[0]) {
        pattern->matrix[i] = 0;
        i++;
    }
    if (dotspos[7] == led_count-1) {
        pattern->matrix[dotspos[7]] = 0;
    }
    if (dotspos[7] == led_count) {
        pattern->matrix[led_count] = 0;
    }
}

/* Run the threaded loop */
void *
matrix_run(void *vargp)
{
    log_matrix_trace("rainbow_run()");

    ws2811_return_t ret;
    struct pattern *pattern = (struct pattern*)vargp;

    /* This should never get called before load_rainbox_pattern initializes stuff.
     * Or ever be called after kill_pattern_rainbox */
    assert(pattern->running);
    while (pattern->running)
    {
        /* If the pattern is paused, we won't update anything */
        if (!pattern->paused) {
            matrix_bottom(pattern);
            matrix_render(pattern);
            if ((ret = ws2811_render(pattern->ledstring)) != WS2811_SUCCESS)
            {
                log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                // XXX: This should cause some sort of fatal error to propogate upwards
                break;
            }
        }
        // 15 frames /sec
        usleep(1000000 / pattern->movement_rate);
    }

    return NULL;
}

/* Initialize everything, and begin the thread */
ws2811_return_t
rainbow_load(struct pattern *pattern)
{
    log_trace("rainbow_load()");

    /* Allocate memory */
    pattern->matrix = calloc(pattern->ledstring->channel[0].count, sizeof(ws2811_led_t));
    assert(pattern->matrix != NULL);
    /* A protection against matrix_run() being called in a bad order. */
    pattern->running = 1;

    pthread_create(&pattern->thread_id, NULL, matrix_run, pattern);
    log_info("Rainbow Pattern Loop is now running.");
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_start(struct pattern *pattern)
{
    log_trace("rainbow_start()");
    pattern->paused = false;
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_stop(struct pattern *pattern)
{
    log_trace("rainbow_stop()");
    pattern->paused = true;
    matrix_clear(pattern);
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_pause(struct pattern *pattern)
{
    log_trace("rainbow_pause()");
    pattern->paused = true;
    return WS2811_SUCCESS;
}

/* Turn off strip, kill second process */
ws2811_return_t
rainbow_kill(struct pattern *pattern)
{
    log_trace("rainbow_kill()");

    log_debug("Rainbow Pattern Loop: Stopping run");
    pattern->running = 0;

    log_debug("Rainbow Pattern Loop: Waiting for thread %d to end", pattern->thread_id);
    pthread_join(pattern->thread_id, NULL);

    if (pattern->clear_on_exit) {
        log_info("Raindow Pattern Loop: Clearing matrix");
        matrix_clear(pattern);
        matrix_render(pattern);
        ws2811_render(pattern->ledstring);
    }

    log_info("Rainbow Pattern Loop: now stopped");
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_create(struct pattern *pattern)
{
    log_trace("rainbow_create()");
    pattern->func_load_pattern = &rainbow_load;
    pattern->func_start_pattern = &rainbow_start;
    pattern->func_kill_pattern = &rainbow_kill;
    pattern->func_pause_pattern = &rainbow_pause;
    pattern->running = true;
    pattern->paused = true;
    return WS2811_SUCCESS;
}   

ws2811_return_t
rainbow_delete(struct pattern *pattern)
{
    log_trace("rainbow_delete()");
    log_debug("Rainbow Pattern: Freeing objects");
    free(pattern->matrix);
    pattern->matrix = NULL;
    return WS2811_SUCCESS;
}
