/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2020, HENSOLDT Cyber GmbH
 * Copyright 2022, Capgemini Engineering
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once


/* official device names */
enum chardev_id {
    LS_DUART_UART1,
    LS_DUART_UART2,
    LS_DUART_UART3,
    LS_DUART_UART4,
    /* Aliases */
    PS_SERIAL0 = LS_DUART_UART1,
    PS_SERIAL1 = LS_DUART_UART2,
    PS_SERIAL2 = LS_DUART_UART3,
    PS_SERIAL3 = LS_DUART_UART4,

#if defined(CONFIG_PLAT_LS1043A_RDB)

    PS_SERIAL_DEFAULT = LS_DUART_UART1

#else
#error "unknown ls1043 platform selected!"
#endif

};
