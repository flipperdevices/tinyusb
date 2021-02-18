/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber - rework to unmasked pointers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

/** \ingroup Group_Common
 * \defgroup group_fifo fifo
 *  @{ */

#ifndef _TUSB_FIFO_H_
#define _TUSB_FIFO_H_

// Due to the use of unmasked pointers, this FIFO does not suffer from loosing
// one item slice. Furthermore, write and read operations are completely
// decoupled as write and read functions do not modify a common state. Henceforth,
// writing or reading from the FIFO within an ISR is safe as long as no other
// process (thread or ISR) interferes.
// Also, this FIFO is ready to be used in combination with a DMA as the write and
// read pointers can be updated from within a DMA ISR. Overflows are detectable
// within a certain number (see tu_fifo_overflow()).

// mutex is only needed for RTOS
// for OS None, we don't get preempted
#define CFG_FIFO_MUTEX      (CFG_TUSB_OS != OPT_OS_NONE)

#include <stdint.h>
#include <stdbool.h>
#include "osal/osal.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CFG_FIFO_MUTEX
#define tu_fifo_mutex_t  osal_mutex_t
#endif

/** \enum tu_fifo_copy_mode_t
 * \brief Write modes intended to allow special read and write functions to be able to copy data to and from USB hardware FIFOs as needed for e.g. STM32s
 */
typedef enum
{
  TU_FIFO_COPY_INC,                     ///< Copy from/to an increasing source/destination address - default mode
  TU_FIFO_COPY_CST,                     ///< Copy from/to a constant source/destination address - required for e.g. STM32 to write into USB hardware FIFO
} tu_fifo_copy_mode_t;

/** \struct tu_fifo_t
 * \brief Simple Circular FIFO
 */
typedef struct
{
  uint8_t* buffer                        ; ///< buffer pointer
  uint16_t depth                         ; ///< max items
  uint16_t item_size                     ; ///< size of each item
  bool overwritable                      ;

  uint16_t non_used_index_space          ; ///< required for non-power-of-two buffer length
  uint16_t max_pointer_idx               ; ///< maximum absolute pointer index

  volatile uint16_t wr_idx               ; ///< write pointer
  volatile uint16_t rd_idx               ; ///< read pointer

  tu_fifo_copy_mode_t wr_mode            ; ///< write mode - default is TU_FIFO_COPY_INC
  tu_fifo_copy_mode_t rd_mode            ; ///< read mode - default is TU_FIFO_COPY_INC

#if CFG_FIFO_MUTEX
  tu_fifo_mutex_t mutex;
#endif

} tu_fifo_t;

#define TU_FIFO_DEF(_name, _depth, _type, _overwritable)                \
    uint8_t _name##_buf[_depth*sizeof(_type)];                          \
    tu_fifo_t _name = {                                                 \
        .buffer                 = _name##_buf,                          \
        .depth                  = _depth,                               \
        .item_size              = sizeof(_type),                        \
        .overwritable           = _overwritable,                        \
        .max_pointer_idx        = 2*_depth-1,                           \
        .non_used_index_space   = UINT16_MAX - (2*_depth-1),            \
        .wr_mode                = TU_FIFO_COPY_INC,                     \
        .rd_mode                = TU_FIFO_COPY_INC,                     \
    }

bool tu_fifo_set_overwritable(tu_fifo_t *f, bool overwritable);
bool tu_fifo_clear(tu_fifo_t *f);
bool tu_fifo_config(tu_fifo_t *f, void* buffer, uint16_t depth, uint16_t item_size, bool overwritable);

#if CFG_FIFO_MUTEX
static inline void tu_fifo_config_mutex(tu_fifo_t *f, tu_fifo_mutex_t mutex_hdl)
{
  f->mutex = mutex_hdl;
}
#endif

bool     tu_fifo_write                  (tu_fifo_t* f, void const * p_data);
uint16_t tu_fifo_write_n                (tu_fifo_t* f, void const * p_data, uint16_t n);

bool     tu_fifo_read                   (tu_fifo_t* f, void * p_buffer);
uint16_t tu_fifo_read_n                 (tu_fifo_t* f, void * p_buffer, uint16_t n);
uint16_t tu_fifo_read_n_into_other_fifo (tu_fifo_t* f, tu_fifo_t* f_target, uint16_t offset, uint16_t n);

bool     tu_fifo_peek_at                (tu_fifo_t* f, uint16_t pos, void * p_buffer);
uint16_t tu_fifo_peek_at_n              (tu_fifo_t* f, uint16_t pos, void * p_buffer, uint16_t n);
uint16_t tu_fifo_peek_n_into_other_fifo (tu_fifo_t* f, tu_fifo_t* f_target, uint16_t offset, uint16_t n);

uint16_t tu_fifo_count                  (tu_fifo_t* f);
bool     tu_fifo_empty                  (tu_fifo_t* f);
bool     tu_fifo_full                   (tu_fifo_t* f);
uint16_t tu_fifo_remaining              (tu_fifo_t* f);
bool     tu_fifo_overflowed             (tu_fifo_t* f);
void     tu_fifo_correct_read_pointer   (tu_fifo_t* f);

// Pointer modifications intended to be used in combinations with DMAs.
// USE WITH CARE - NO SAFTY CHECKS CONDUCTED HERE! NOT MUTEX PROTECTED!
void     tu_fifo_advance_write_pointer  (tu_fifo_t *f, uint16_t n);
void     tu_fifo_backward_write_pointer (tu_fifo_t *f, uint16_t n);
void     tu_fifo_advance_read_pointer   (tu_fifo_t *f, uint16_t n);
void     tu_fifo_backward_read_pointer  (tu_fifo_t *f, uint16_t n);

// If you want to read/write from/to the FIFO by use of a DMA, you may need to conduct two copies to handle a possible wrapping part
// This functions deliver a pointer to start reading/writing from/to and a valid linear length along which no wrap occurs.
// In case not all of your data is available within one read/write, update the read/write pointer by
// tu_fifo_advance_read_pointer()/tu_fifo_advance_write_pointer and conduct a second read/write operation
uint16_t tu_fifo_get_linear_read_info   (tu_fifo_t *f, uint16_t offset, void **ptr, uint16_t n);
uint16_t tu_fifo_get_linear_write_info  (tu_fifo_t *f, uint16_t offset, void **ptr, uint16_t n);

static inline bool tu_fifo_peek(tu_fifo_t* f, void * p_buffer)
{
  return tu_fifo_peek_at(f, 0, p_buffer);
}

static inline uint16_t tu_fifo_depth(tu_fifo_t* f)
{
  return f->depth;
}

// When writing into the FIFO by fifo_write_n(), rd_mode determines how the pointer read from is modified
static inline void tu_fifo_set_copy_mode_read(tu_fifo_t* f, tu_fifo_copy_mode_t rd_mode)
{
  f->rd_mode = rd_mode;
}

// When reading from the FIFO by fifo_read_n() or fifo_peek_n(), wr_mode determines how the pointer written to is modified
static inline void tu_fifo_set_copy_mode_write(tu_fifo_t* f, tu_fifo_copy_mode_t wr_mode)
{
  f->wr_mode = wr_mode;
}

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_FIFO_H_ */
