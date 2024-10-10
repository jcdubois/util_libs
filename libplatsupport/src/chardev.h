/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <platsupport/chardev.h>
#include <utils/arith.h>

/**
 * Struct representing the definition of a device.
 *
 * @id      - Identifier for the character device.
 * @paddr   - The physical address of the device.
 * @size    - The size of the memory region for the device.
 * @irqs    - Pointer to an array of interrupt request numbers (IRQs)
 *            associated with the device. The array is terminated by the
 *            -1 value.
 * @init_fn - Function pointer to an initialization function for the device.
 */
struct dev_defn {
    enum chardev_id id;
    uintptr_t paddr;
    int size;
    const int *irqs;
    int (*init_fn)(const struct dev_defn *defn,
                   const ps_io_ops_t *ops,
                   struct ps_chardevice *dev);
};

/**
 * Function to map the physical memory region of a character device to
 * virtual memory.
 *
 * @param dev - Pointer to the device definition structure.
 * @param ops - Pointer to the I/O operations structure for mapping.
 *
 * @return A pointer to the mapped virtual memory address of the device.
 */
static inline void *chardev_map(
    const struct dev_defn *dev,
    const ps_io_ops_t *ops)
{
    return ps_io_map(
               &ops->io_mapper,         // I/O mapper from the ops structure
               dev->paddr,              // Device's physical address
               dev->size,               // Size of the memory to map
               0,                       // Uncached mapping
               PS_MEM_NORMAL);          // Normal memory
}

/**
 * Function to initialize a UART device.
 *
 * @param defn - Pointer to the device definition structure.
 * @param ops  - Pointer to the I/O operations structure.
 * @param dev  - Pointer to the character device structure to initialize.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int uart_init(
    const struct dev_defn *defn,
    const ps_io_ops_t *ops,
    ps_chardevice_t *dev);

/**
 * Function to initialize a UART device with a statically provided virtual
 * address.
 *
 * @param vaddr - Virtual address of the mapped UART device.
 * @param ops   - Pointer to the I/O operations structure.
 * @param dev   - Pointer to the character device structure to initialize.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int uart_static_init(
    void *vaddr,
    const ps_io_ops_t *ops,
    ps_chardevice_t *dev);

/**
 * Function to write data to a UART device.
 *
 * @param dev   - Pointer to the character device structure.
 * @param vdata - Pointer to the data to write.
 * @param count - Number of bytes to write.
 * @param rcb   - (Unused) Callback function for write completion.
 * @param token - (Unused) Token for callback identification.
 *
 * @return Number of bytes written, or a negative error code on failure.
 */
ssize_t uart_write(
    ps_chardevice_t *dev,
    const void *vdata,
    size_t count,
    chardev_callback_t rcb UNUSED,
    void *token UNUSED);

/**
 * Function to read data from a UART device.
 *
 * @param dev   - Pointer to the character device structure.
 * @param vdata - Pointer to the buffer to store the read data.
 * @param count - Number of bytes to read.
 * @param rcb   - (Unused) Callback function for read completion.
 * @param token - (Unused) Token for callback identification.
 *
 * @return Number of bytes read, or a negative error code on failure.
 */
ssize_t uart_read(
    ps_chardevice_t *dev,
    void *vdata,
    size_t count,
    chardev_callback_t rcb UNUSED,
    void *token UNUSED);

/**
 * Function to get a single character from a UART device.
 *
 * @param dev - Pointer to the character device structure.
 *
 * @return The character read, or a negative error code on failure.
 */
int uart_getchar(
    ps_chardevice_t *dev);

/**
 * Function to send a single character to a UART device.
 *
 * @param dev - Pointer to the character device structure.
 * @param c   - The character to send.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int uart_putchar(
    ps_chardevice_t *dev,
    int c);

