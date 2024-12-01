/*
 * Copyright 2024, Jean-Christophe Dubois
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define FTM0_PATH "/soc/timer@29d0000"
#define FTM1_PATH "/soc/timer@29e0000"
#define FTM2_PATH "/soc/timer@29f0000"
#define FTM3_PATH "/soc/timer@2a00000"
#define FTM4_PATH "/soc/timer@2a10000"
#define FTM5_PATH "/soc/timer@2a20000"
#define FTM6_PATH "/soc/timer@2a30000"
#define FTM7_PATH "/soc/timer@2a40000"

/* Timers */
typedef enum {
  FTM0_TIMER,
  FTM1_TIMER,
  FTM2_TIMER,
  FTM3_TIMER,
  FTM4_TIMER,
  FTM5_TIMER,
  FTM6_TIMER,
  FTM7_TIMER,
  NTIMERS
} ftm_id_t;

#include <platsupport/mach/timer.h>
