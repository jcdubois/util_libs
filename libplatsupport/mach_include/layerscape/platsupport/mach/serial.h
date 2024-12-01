/*
 * Copyright 2024, Jean-Christophe Dubois
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
#error "unknown layerscape platform selected!"
#endif
};
