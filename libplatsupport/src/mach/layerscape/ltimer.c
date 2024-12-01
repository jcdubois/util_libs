/*
 * Copyright 2024, Jean-Christophe Dubois
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Minimal implementation of a logical timer for layerscape
 *
 * Does not implement some functions yet.
 */
#include <platsupport/ltimer.h>
#include <platsupport/plat/timer.h>
#include <platsupport/timer.h>

#include <utils/util.h>

#include "../../ltimer.h"

/* Use ftm0_timer for timeouts/sleep */
#define FTM_TIMEOUT FTM0_TIMER
/* Use ftm1_timer to keep running for timestamp/gettime */
#define FTM_TIMESTAMP FTM1_TIMER

#define N_FTMS 2
#define TIMEOUT_IDX 0
#define TIMESTAMP_IDX 1

typedef struct {
  ftm_t ftms[N_FTMS];
  ps_io_ops_t ops;
  bool timeout_initialised;
  bool timestamp_initialised;
} ftm_ltimer_t;

static int get_time(void *data, uint64_t *time) {
  assert(data != NULL);
  assert(time != NULL);
  ftm_ltimer_t *ftm_ltimer = data;
  *time = ftm_get_time(&ftm_ltimer->ftms[TIMESTAMP_IDX]);
  return 0;
}

static int get_resolution(void *data, uint64_t *resolution) { return ENOSYS; }

static int set_timeout(void *data, uint64_t ns, timeout_type_t type) {
  assert(data != NULL);
  ftm_ltimer_t *ftm_ltimer = data;

  if (type == TIMEOUT_ABSOLUTE) {
    uint64_t time = 0;
    get_time(data, &time);
    if (ns <= time) {
      return ETIME;
    } else {
      ns -= time;
    }
  }

  return ftm_set_timeout(&ftm_ltimer->ftms[TIMEOUT_IDX], ns,
                         type == TIMEOUT_PERIODIC);
}

static int reset(void *data) {
  assert(data != NULL);
  ftm_ltimer_t *ftm_ltimer = data;

  /* reset the timers */
  ftm_stop(&ftm_ltimer->ftms[TIMEOUT_IDX]);
  ftm_start(&ftm_ltimer->ftms[TIMEOUT_IDX]);
  ftm_stop(&ftm_ltimer->ftms[TIMESTAMP_IDX]);
  ftm_start(&ftm_ltimer->ftms[TIMESTAMP_IDX]);

  return 0;
}

static void destroy(void *data) {
  assert(data);

  ftm_ltimer_t *ftm_ltimer = data;

  int error = 0;

  if (ftm_ltimer->timeout_initialised) {
    error = ftm_destroy(&ftm_ltimer->ftms[TIMEOUT_IDX]);
    ZF_LOGF_IF(error, "Failed to de-allocate the timeout timer");
  }

  if (ftm_ltimer->timestamp_initialised) {
    error = ftm_destroy(&ftm_ltimer->ftms[TIMESTAMP_IDX]);
    ZF_LOGF_IF(error, "Failed to de-allocate the timestamp timer");
  }

  ps_free(&ftm_ltimer->ops.malloc_ops, sizeof(ftm_ltimer), ftm_ltimer);
}

static int create_ltimer(ltimer_t *ltimer, ps_io_ops_t ops) {
  ltimer->get_time = get_time;
  ltimer->get_resolution = get_resolution;
  ltimer->set_timeout = set_timeout;
  ltimer->reset = reset;
  ltimer->destroy = destroy;

  int error =
      ps_calloc(&ops.malloc_ops, 1, sizeof(ftm_ltimer_t), &ltimer->data);
  if (error) {
    return error;
  }
  assert(ltimer->data != NULL);

  return 0;
}

int ltimer_default_init(ltimer_t *ltimer, ps_io_ops_t ops,
                        ltimer_callback_fn_t callback, void *callback_token) {
  int error = create_ltimer(ltimer, ops);
  if (error) {
    return error;
  }

  ftm_ltimer_t *ftm_ltimer = ltimer->data;
  ftm_ltimer->ops = ops;

  ftm_config_t config = {
      .io_ops = ops,
      .user_callback = callback,
      .user_callback_token = callback_token,
      .is_timestamp = false,
      .id = FTM_TIMEOUT,
  };

  ftm_config_t config_timestamp = {
      .io_ops = ops,
      .user_callback = callback,
      .user_callback_token = callback_token,
      .is_timestamp = true,
      .id = FTM_TIMESTAMP,
  };

  error = ftm_init(&ftm_ltimer->ftms[TIMEOUT_IDX], config);
  if (error) {
    ZF_LOGE("Failed to init the timeout timer");
    ltimer_destroy(ltimer);
    return error;
  }

  ftm_ltimer->timeout_initialised = true;

  error = ftm_start(&ftm_ltimer->ftms[TIMEOUT_IDX]);
  if (error) {
    ZF_LOGE("Failed to start the timeout timer");
    ltimer_destroy(ltimer);
    return error;
  }

  /* set the second ftm to be a timestamp counter */
  error = ftm_init(&ftm_ltimer->ftms[TIMESTAMP_IDX], config_timestamp);
  if (error) {
    ZF_LOGE("Failed to init the timestamp timer");
    ltimer_destroy(ltimer);
    return error;
  }

  ftm_ltimer->timestamp_initialised = true;

  error = ftm_freerun(&ftm_ltimer->ftms[TIMESTAMP_IDX]);
  if (error) {
    ZF_LOGE("Failed to start the timestamp timer");
    ltimer_destroy(ltimer);
    return error;
  }

  return 0;
}

/* This function is intended to be deleted,
 * this is just left here for now so that stuff can compile */
int ltimer_default_describe(ltimer_t *ltimer, ps_io_ops_t ops) {
  ZF_LOGE("get_(nth/num)_(irqs/pmems) are not valid");
  return EINVAL;
}
