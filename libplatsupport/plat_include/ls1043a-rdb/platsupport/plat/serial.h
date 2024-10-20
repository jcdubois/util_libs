/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2022, Capgemini Engineering
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <autoconf.h>
#include <platsupport/mach/serial.h>

#define UART1_PADDR  0x021C0500
#define UART2_PADDR  0x021C0600
#define UART3_PADDR  0x021D0500
#define UART4_PADDR  0x021D0600

#define UART1_IRQ    86
#define UART2_IRQ    86
#define UART3_IRQ    87
#define UART4_IRQ    87

#define UART_REF_CLK 100000000

#if defined(CONFIG_PLAT_LS1043A_RDB)
#define DEFAULT_SERIAL_PADDR        UART1_PADDR
#define DEFAULT_SERIAL_INTERRUPT    UART1_IRQ
#endif
