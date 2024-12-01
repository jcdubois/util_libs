/*
 * Copyright 2024, Jean-Christophe Dubois
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <platsupport/clock.h>
#include <platsupport/fdt.h>
#include <platsupport/io.h>
#include <platsupport/ltimer.h>
#include <platsupport/timer.h>

#define IRQS_PER_FTM 1

#define TMR_DEFAULT FTM0_TIMER

typedef struct {
  bool is_timestamp;
  ps_io_ops_t io_ops;
  ltimer_callback_fn_t user_callback;
  void *user_callback_token;
  char *device_path;
  ftm_id_t id;
} ftm_config_t;

typedef struct {
  ps_io_ops_t io_ops;
  irq_id_t irq_id;
  pmem_region_t timer_pmem;
  ltimer_callback_fn_t user_callback;
  void *user_callback_token;
  void *regs;
  uint64_t hi_time;
  freq_t freq;
  ftm_id_t id;
  bool is_timestamp;
  bool is_periodic;
} ftm_t;

static UNUSED timer_properties_t ftm_properties = {
    .upcounter = true,
    .timeouts = true,
    .bit_width = 16,
    .irqs = 1,
    .relative_timeouts = true,
    .absolute_timeouts = false,
};

/**
 * init the passed ftm timer with the provided config
 */
int ftm_init(ftm_t *ftm, ftm_config_t config);
/**
 * unmap the ftm timer and unregister it from interrupts
 */
int ftm_destroy(ftm_t *ftm);
/**
 * Start the ftm timer
 */
int ftm_start(ftm_t *ftm);
/**
 * Stop the ftm timer
 */
int ftm_stop(ftm_t *ftm);
/**
 * Set a reltive timeout
 */
int ftm_set_timeout(ftm_t *ftm, uint64_t ns, bool periodic);
/**
 * set the ftm to 0 and start free running, where the timer will
 * continually increment and trigger irqs on each overflow and reload to 0
 */
int ftm_freerun(ftm_t *ftm);
/**
 * Read the value of the current time in ns
 */
uint64_t ftm_get_time(ftm_t *ftm);
/**
 * convert from a ticks value to ns for a configured ftm
 */
uint64_t ftm_ticks_to_ns(ftm_t *ftm, uint32_t ticks);
