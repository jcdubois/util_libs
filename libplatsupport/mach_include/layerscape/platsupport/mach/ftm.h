/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <platsupport/io.h>
#include <platsupport/ltimer.h>
#include <platsupport/fdt.h>
#include <platsupport/timer.h>
#include <platsupport/plat/ftm_constants.h>

#include <stdint.h>

typedef struct {
    /* initialised IO ops structure */
    ps_io_ops_t io_ops;
    /* user callback function to be called on interrupt */
    ltimer_callback_fn_t user_callback;
    /* token to be passed into the callback function */
    void *user_callback_token;
    /* path to the gpt node in the DTB */
    char *device_path;
    /* prescaler to scale time by. 0 = divide by 1. 1 = divide by 2. ...*/
    uint32_t prescaler;
} ftm_config_t;

struct ftm_map;

typedef struct gpt {
    ps_io_ops_t io_ops;
    irq_id_t irq_id;
    ltimer_callback_fn_t user_callback;
    void *user_callback_token;
    pmem_region_t timer_pmem;
    volatile struct ftm_map *ftm_map;
    uint32_t prescaler;
    uint32_t high_bits;
} ftm_t;

/* More can be done with this timer
 * but this driver can only count up
 * currently.
 */
static inline timer_properties_t ftm_get_properies(void)
{
    return (timer_properties_t) {
        .upcounter = true,
        .timeouts = true,
        .absolute_timeouts = false,
        .relative_timeouts = true,
        .periodic_timeouts = true,
        .bit_width = 32,
        .irqs = 1,
    };
}

/*
 * Initialise a passed in gpt struct with the provided config
 */
int ftm_init(ftm_t *ftm, ftm_config_t config);

/* start the gpt */
int ftm_start(ftm_t *ftm);
/* stop the gpt */
int ftm_stop(ftm_t  *ftm);
/* destroy the gpt */
int ftm_destroy(ftm_t *ftm);
/* read the value of the current time in ns */
uint64_t ftm_get_time(ftm_t *ftm);
/* set a relative timeout */
int ftm_set_timeout(ftm_t *ftm, uint64_t ns, bool periodic);

