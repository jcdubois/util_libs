/*
 * Copyright 2024, Jean-Christophe Dubois
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "../../chardev.h"
#include <platsupport/serial.h>
#include <stdlib.h>
#include <string.h>

// NS16550A Register Offsets (UART device registers)
#define UART_RBR 0x00 // Receiver Buffer Register
#define UART_THR 0x00 // Transmitter Holding Register
#define UART_IER 0x01 // Interrupt Enable Register
#define UART_LSR 0x05 // Line Status Register

#define UART_LSR_THRE BIT(5) // Transmitter Holding Register Empty
#define UART_LSR_DR BIT(0)   // Data Ready

#define UART_RBR_MASK MASK(8) // Get only the 8 lower bits

#define UART_IER_ERDAI BIT(0) // Enable receive interrupts

#define REG_PTR(base, offset) ((volatile uint8_t *)((char *)(base) + (offset)))

// Function to read a single character from the UART
int uart_getchar(ps_chardevice_t *d) {
  int ch = EOF;

  // if data is available in the Receiver Buffer Register
  if (*REG_PTR(d->vaddr, UART_LSR) & UART_LSR_DR) {
    // Read the received character from the Receiver Buffer Register
    ch = *REG_PTR(d->vaddr, UART_RBR) & UART_RBR_MASK;
  }

  return ch;
}

// Function to write a single character to the UART
int uart_putchar(ps_chardevice_t *d, int c) {
  // Wait until there is room in the Transmiter Holding Register
  while (!(*REG_PTR(d->vaddr, UART_LSR) & UART_LSR_THRE)) {
    continue;
  }

  // Write the character to the Transmitter Holding Register
  *REG_PTR(d->vaddr, UART_THR) = c;

  // if EOL and SERIAL_AUTO_CR
  if (c == '\n' && (d->flags & SERIAL_AUTO_CR)) {
    // Write the \r character
    uart_putchar(d, '\r');
  }

  return c;
}

static void uart_handle_irq(ps_chardevice_t *d) { /* TODO */
}

int uart_init(const struct dev_defn *defn, const ps_io_ops_t *ops,
              ps_chardevice_t *dev) {
  // Init internal device structure
  memset(dev, 0, sizeof(*dev));

  // Map requested uart device
  void *vaddr = chardev_map(defn, ops);
  if (vaddr == NULL) {
    return -1;
  }

  /* Set up all the  device properties. */
  dev->id = defn->id;
  dev->vaddr = (void *)vaddr;
  dev->read = &uart_read;
  dev->write = &uart_write;
  dev->handle_irq = &uart_handle_irq;
  dev->irqs = defn->irqs;
  dev->ioops = *ops;
  dev->flags = SERIAL_AUTO_CR;

  // Enable receive interrupts
  *REG_PTR(dev->vaddr, UART_IER) = UART_IER_ERDAI;

  return 0;
}
