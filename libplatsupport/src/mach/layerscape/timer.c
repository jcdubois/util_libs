/*
 * Copyright 2024, Jean-Christophe Dubois
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <platsupport/plat/timer.h>
#include <platsupport/timer.h>

#include <byteswap.h>

#define CLEANUP_FAIL_TEXT                                                      \
  "Failed to cleanup the FTM after failing to initialise it"

#define FTM_SC_CLK_MASK_SHIFT 3
#define FTM_SC_CLK_MASK (3 << FTM_SC_CLK_MASK_SHIFT)
#define FTM_SC_TOF 0x80
#define FTM_SC_TOIE 0x40
#define FTM_SC_CPWMS 0x20
#define FTM_SC_CLKS 0x18
#define FTM_SC_PS_1 0x0
#define FTM_SC_PS_2 0x1
#define FTM_SC_PS_4 0x2
#define FTM_SC_PS_8 0x3
#define FTM_SC_PS_16 0x4
#define FTM_SC_PS_32 0x5
#define FTM_SC_PS_64 0x6
#define FTM_SC_PS_128 0x7
#define FTM_SC_PS_MASK 0x7

#define FTM_SC_CLK(c) ((c) << FTM_SC_CLK_MASK_SHIFT)

/*
 * Select Fixed frequency clock (32KHz) as clock source
 * of FlexTimer Module
 */
#define FTM_SC_CLKS_FIXED_FREQ 0x02
#define FIXED_FREQ_CLK 32000

/* Select 128 (2^7) as divider factor */
#define MAX_FREQ_DIV (1 << FTM_SC_PS_MASK)

/* Maximum counter value in FlexTimer's CNT registers */
#define MAX_COUNT_VAL 0xffff

struct ftm_tmr_regs {
  /* Status and control */
  uint32_t sc;
  /* Counter */
  uint32_t cnt;
  /* Modulo */
  uint32_t mod;
  /* 8 channels */
  struct {
    /* Channel status and control */
    uint32_t cxsc;
    /* Channel value */
    uint32_t cxv;
  } channel[8];
  /* Counter initial value */
  uint32_t cntin;
  /* Capture and compare status */
  uint32_t status;
  /* Features mode selection */
  uint32_t mode;
  /* Synchronization */
  uint32_t sync;
  /* Initial state for channel output */
  uint32_t outinit;
  /* Ouptput mask */
  uint32_t outmask;
  /* function for linked channels */
  uint32_t combine;
  /* Dead time insertion control */
  uint32_t deadtime;
  /* FTM external trigger */
  uint32_t exttrig;
  /* channel polarity */
  uint32_t pol;
  /* Fault mode status */
  uint32_t fms;
  /* input capture filter control */
  uint32_t filter;
  /* Fault control */
  uint32_t fltctrl;
  /* Quadrature decoder Control and status */
  uint32_t qdctrl;
  /* Configuration */
  uint32_t conf;
  /* FTM fault input polarity */
  uint32_t fltpol;
  /* Synchronization configuration */
  uint32_t synconf;
  /* FTM inverting control */
  uint32_t invctrl;
  /* FTM software output control */
  uint32_t swoctrl;
  /* FTM PWM load */
  uint32_t pwmload;
};
typedef volatile struct ftm_tmr_regs ftm_tmr_regs_t;

static bool need_to_swap = false;

static inline ftm_tmr_regs_t *ftm_get_regs(ftm_t *ftm) { return ftm->regs; }

static inline char *ftm_get_device_path(ftm_id_t id) {
  switch (id) {
  case FTM0_TIMER:
    return FTM0_PATH;
  case FTM1_TIMER:
    return FTM1_PATH;
  case FTM2_TIMER:
    return FTM2_PATH;
  case FTM3_TIMER:
    return FTM3_PATH;
  case FTM4_TIMER:
    return FTM4_PATH;
  case FTM5_TIMER:
    return FTM5_PATH;
  case FTM6_TIMER:
    return FTM6_PATH;
  case FTM7_TIMER:
    return FTM7_PATH;
  default:
    ZF_LOGF("Invalid ftm_id_t!");
    return NULL;
  }
}

static inline uint32_t ftm_reg_value(uint32_t val) {
  if (need_to_swap) {
    val = bswap_32(val);
  }
  return val;
}

static inline void ftm_clock_enable(ftm_t *ftm) {
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);

  /*
   * Select and enable the 32KHz clock source together with the
   * 128 divider
   */
  uint32_t val = ftm_reg_value(regs->sc);
  val &= ~(FTM_SC_PS_MASK | FTM_SC_CLK_MASK);
  val |= (FTM_SC_PS_MASK | FTM_SC_CLK(FTM_SC_CLKS_FIXED_FREQ));
  regs->sc = ftm_reg_value(val);
}

static inline void ftm_clock_disable(ftm_t *ftm) {
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);

  /*
   * disable the clock source
   */
  uint32_t val = ftm_reg_value(regs->sc);
  val &= ~(FTM_SC_PS_MASK | FTM_SC_CLK_MASK);
  regs->sc = ftm_reg_value(val);
}

static inline bool ftm_irq_acknowledge(ftm_t *ftm) {
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);
  uint32_t timeout = 100;
  bool ret = ftm_reg_value(regs->sc) & FTM_SC_TOF ? true : false;

  /*
   * Fix errata A-007728 for flextimer
   * =================================
   *  If the FTM counter reaches the FTM_MOD value between
   *  the reading of the TOF bit and the writing of 0 to
   *  the TOF bit, the process of clearing the TOF bit
   *  does not work as expected when FTMx_CONF[NUMTOF] != 0
   *  and the current TOF count is less than FTMx_CONF[NUMTOF].
   *  If the above condition is met, the TOF bit remains set.
   *  If the TOF interrupt is enabled (FTMx_SC[TOIE] = 1),the
   *  TOF interrupt also remains asserted.
   *
   *  Above is the errata discription
   *
   *  In one word: software clearing TOF bit does not works when
   *  FTMx_CONF[NUMTOF] was set as nonzero and FTM counter
   *  reaches the FTM_MOD value.
   *
   *  The workaround is clearing TOF bit until it works
   *  (FTM counter doesn't always reach the FTM_MOD anyway),
   *  which may cost some cycles.
   */
  while ((FTM_SC_TOF & ftm_reg_value(regs->sc)) && timeout--) {
    regs->sc &= ftm_reg_value(~FTM_SC_TOF);
  }

  return ret;
}

static inline void ftm_irq_enable(ftm_t *ftm) {
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);

  uint32_t val = ftm_reg_value(regs->sc);
  val |= FTM_SC_TOIE;
  regs->sc = ftm_reg_value(val);
}

static inline void ftm_irq_disable(ftm_t *ftm) {
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);

  uint32_t val = ftm_reg_value(regs->sc);
  val &= ~FTM_SC_TOIE;
  regs->sc = ftm_reg_value(val);
}

static void ftm_clean_alarm(ftm_t *ftm) {
  ftm_clock_disable(ftm);
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);

  regs->cntin = 0;
  regs->mod = ftm_reg_value(MAX_COUNT_VAL);

  /*
   * The CNT register contains the FTM counter value.
   * Reset clears the CNT register. Writing any value to COUNT
   * updates the counter with its initial value, CNTIN.
   */
  regs->cnt = 0;
}

/****************** Clocks ******************/

static inline freq_t ftm_get_freq(ftm_t *ftm) { return ftm->freq; }

/********************************************/

int ftm_start(ftm_t *ftm) {
  if (ftm == NULL) {
    ZF_LOGE("Invalid (null) argument for ftm timer start");
    return EINVAL;
  }

  ftm_clock_enable(ftm);
  ftm_irq_enable(ftm);

  return 0;
}

int ftm_stop(ftm_t *ftm) {
  if (ftm == NULL) {
    ZF_LOGE("Invalid (null) argument for ftm timer stop");
    return EINVAL;
  }

  ftm_clock_disable(ftm);
  ftm_irq_disable(ftm);

  return 0;
}

int ftm_freerun(ftm_t *ftm) {
  if (ftm == NULL) {
    ZF_LOGE("Invalid (null) argument for ftm timer freerun");
    return EINVAL;
  }

  return ftm_set_timeout(ftm, MAX_COUNT_VAL, true);
}

static void ftm_handle_irq(void *data, ps_irq_acknowledge_fn_t acknowledge_fn,
                           void *ack_data) {
  assert(data != NULL);
  ftm_t *ftm = (ftm_t *)data;
  bool interrupt_pending = ftm_irq_acknowledge(ftm);

  if (ftm->is_timestamp) {
    /*
     * Check if we already updated the timestamp when reading the time,
     * the interrupt status register should be empty if we did
     */
    if (interrupt_pending) {
      ftm_tmr_regs_t *regs = ftm_get_regs(ftm);
      ftm->hi_time += ftm_ticks_to_ns(ftm, ftm_reg_value(regs->mod));
    }
  } else if (ftm->is_periodic == false) {
    ftm_irq_disable(ftm);
    ftm_clean_alarm(ftm);
  }

  /*
   * Acknowledge the interrupt and call the user callback if any
   */
  ZF_LOGF_IF(acknowledge_fn(ack_data),
             "Failed to acknowledge the interrupt from the ftm");
  if (ftm->user_callback) {
    if (ftm->is_timestamp) {
      ftm->user_callback(ftm->user_callback_token, LTIMER_OVERFLOW_EVENT);
    } else {
      ftm->user_callback(ftm->user_callback_token, LTIMER_TIMEOUT_EVENT);
    }
  }
}

uint64_t ftm_ticks_to_ns(ftm_t *ftm, uint32_t ticks) {
  if (ftm == NULL) {
    return 0;
  }

  uint32_t fin = ftm_get_freq(ftm);

  return freq_cycles_and_hz_to_ns(ticks, fin);
}

/*
 * We return time only on the timestamping timer.
 * Returning time on the timeout timer would not make a lot of sense.
 */
uint64_t ftm_get_time(ftm_t *ftm) {
  if (ftm->is_timestamp) {
    uint32_t fin = ftm_get_freq(ftm);
    ftm_tmr_regs_t *regs = ftm_get_regs(ftm);
    uint32_t cnt = ftm_reg_value(regs->cnt);
    bool interrupt_pending = ftm_irq_acknowledge(ftm);

    /*
     * Check if there is an interrupt pending, i.e. counter overflowed
     */
    if (interrupt_pending) {
      /*
       * Re-read the counter again
       */
      cnt = ftm_reg_value(regs->cnt);
      /*
       * Bump the hi_time counter now, as there may be latency in serving the
       * interrupt
       */
      ftm->hi_time += ftm_ticks_to_ns(ftm, ftm_reg_value(regs->mod));
    }

    return ftm->hi_time + freq_cycles_and_hz_to_ns(cnt, fin);
  } else {
    return 0;
  }
}

/*
 * 1. Select fixed frequency clock (32KHz) as clock source;
 * 2. Select 128 (2^7) as divider factor;
 * So clock is 250 Hz (32KHz/128).
 *
 * 3. FlexTimer's CNT register is a 32bit register,
 * but the register's 16 bit as counter value,it's other 16 bit
 * is reserved.So:
 * - minimum counter value is 0x0
 * - maximum counter value is 0xffff (MAX_COUNT_VAL).
 * So max alarm value is 262 (65536 / 250) seconds
 */
int ftm_set_timeout(ftm_t *ftm, uint64_t ns, bool periodic) {
  if (ftm == NULL) {
    ZF_LOGE("Invalid (null) argument for ftm timeout setting");
    return EINVAL;
  }

  /*
   * Compute the interval value
   */
  uint32_t fin = ftm_get_freq(ftm);
  uint64_t interval = freq_ns_and_hz_to_cycles(ns, fin);

  if (interval > MAX_COUNT_VAL) {
    return ETIME;
  }

  ftm_clean_alarm(ftm);
  ftm_irq_disable(ftm);

  ftm->is_periodic = periodic;

  /*
   * The counter increments until the value of MOD is reached,
   * at which point the counter is reloaded with the value of CNTIN.
   * The TOF (the overflow flag) bit is set when the FTM counter
   * changes from MOD to CNTIN. So we should use the interval - 1.
   */
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);
  regs->mod = ftm_reg_value(interval > 1 ? interval - 1 : 1);

  ftm_clock_enable(ftm);
  ftm_irq_enable(ftm);

  return 0;
}

static int allocate_register_callback(pmem_region_t pmem, unsigned curr_num,
                                      size_t num_regs, void *token) {
  assert(token != NULL);

  ftm_t *ftm = token;

  ftm->regs = ps_pmem_map(&ftm->io_ops, pmem, false, PS_MEM_NORMAL);
  if (ftm->regs == NULL) {
    ZF_LOGE("Failed to map in registers for the ftm");
    return EIO;
  }

  ftm->timer_pmem = pmem;

  return 0;
}

static int allocate_irq_callback(ps_irq_t irq, unsigned curr_num,
                                 size_t num_irqs, void *token) {
  assert(token != NULL);
  assert(curr_num == 0);
  assert(num_irqs == IRQS_PER_FTM);

  ftm_t *ftm = (ftm_t *)token;
  /*
   * Get the corresponding IRQ position and register the IRQ if it matches
   */
  ftm->irq_id = ps_irq_register(&ftm->io_ops.irq_ops, irq, ftm_handle_irq, ftm);
  if (ftm->irq_id < 0) {
    ZF_LOGE("Failed to register the IRQ for ftm timer %d", ftm->id);
    return EIO;
  }

  return 0;
}

/*
 * We are going to check endianness using the MOD register.
 * This is a 32 bits registers but you can only write the low 16 bits.
 */
static inline void ftm_check_endianness(ftm_t *ftm) {
  ftm_tmr_regs_t *regs = ftm_get_regs(ftm);

  /*
   * We write 32 bits in the MOD register
   */
  regs->mod = 0xffffffff;

  /*
   * When reading it back we check if it value is the expected value
   */
  if (regs->mod == MAX_COUNT_VAL) {
    need_to_swap = false;
  } else {
    need_to_swap = true;
  }
}

int ftm_init(ftm_t *ftm, ftm_config_t config) {
  if (ftm == NULL) {
    ZF_LOGE("Invalid (null) argument for ftm timer init");
    return EINVAL;
  }

  /*
   * Initialise all the struct members
   */
  ftm->io_ops = config.io_ops;
  ftm->user_callback = config.user_callback;
  ftm->user_callback_token = config.user_callback_token;
  ftm->irq_id = PS_INVALID_IRQ_ID;
  ftm->is_timestamp = config.is_timestamp;
  ftm->is_periodic = ftm->is_timestamp ? true : false;
  ftm->id = config.id;
  ftm->hi_time = 0;

  char *device_path = ftm_get_device_path(config.id);

  /*
   * Read the timer's path in the DTB
   */
  ps_fdt_cookie_t *cookie = NULL;
  int error = ps_fdt_read_path(&ftm->io_ops.io_fdt, &ftm->io_ops.malloc_ops,
                               device_path, &cookie);
  if (error) {
    ZF_LOGF_IF(ps_fdt_cleanup_cookie(&ftm->io_ops.malloc_ops, cookie),
               CLEANUP_FAIL_TEXT);
    ZF_LOGF_IF(ftm_destroy(ftm), CLEANUP_FAIL_TEXT);
    return ENODEV;
  }

  /*
   * Walk the registers and allocate them
   */
  error = ps_fdt_walk_registers(&ftm->io_ops.io_fdt, cookie,
                                allocate_register_callback, ftm);
  if (error) {
    ZF_LOGF_IF(ps_fdt_cleanup_cookie(&ftm->io_ops.malloc_ops, cookie),
               CLEANUP_FAIL_TEXT);
    ZF_LOGF_IF(ftm_destroy(ftm), CLEANUP_FAIL_TEXT);
    return ENODEV;
  }

  /*
   * Walk the interrupts and allocate the corresponding interrupt for
   * this timer
   */
  error =
      ps_fdt_walk_irqs(&ftm->io_ops.io_fdt, cookie, allocate_irq_callback, ftm);
  if (error) {
    ZF_LOGF_IF(ps_fdt_cleanup_cookie(&ftm->io_ops.malloc_ops, cookie),
               CLEANUP_FAIL_TEXT);
    ZF_LOGF_IF(ftm_destroy(ftm), CLEANUP_FAIL_TEXT);
    return ENODEV;
  }

  /*
   * According to the linux device tree the FlexTimer might have an
   * endianess different from the processor. We could rely on the device
   * tree (like linux does) as the device node could have a "big-endian" or
   * a "little-endian" property.
   * But it seems as easier and faster to actualy test for it directly on
   * the device.
   */
  ftm_check_endianness(ftm);

  /*
   * We use the fixed clock only (at least for now)
   */
  ftm->freq = (uint32_t)FIXED_FREQ_CLK / (uint32_t)MAX_FREQ_DIV;

  /*
   * Disable clock and reset register values
   */
  ftm_irq_disable(ftm);
  ftm_clean_alarm(ftm);

  return 0;
}

int ftm_destroy(ftm_t *ftm) {
  if (ftm == NULL) {
    ZF_LOGE("Invalid (null) argument for ftm timer destroy");
    return EINVAL;
  }

  if (ftm->regs) {
    ZF_LOGF_IF(ftm_stop(ftm), "Failed to stop the ftm before de-allocating it");
    ps_io_unmap(&ftm->io_ops.io_mapper, ftm->regs,
                (size_t)ftm->timer_pmem.length);
  }

  if (ftm->irq_id != PS_INVALID_IRQ_ID) {
    ZF_LOGF_IF(ps_irq_unregister(&ftm->io_ops.irq_ops, ftm->irq_id),
               "Failed to unregister IRQ");
  }

  return 0;
}
