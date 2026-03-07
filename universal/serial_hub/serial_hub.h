#ifndef SERIAL_HUB_H
#define SERIAL_HUB_H

#ifdef __cplusplus
extern "C" {
#endif
#include "stdint.h"

#define SERIAL_HUB_TOPIC_MAX 8

typedef uint16_t fsize_t;

typedef uint16_t fssize_t;

typedef void (*on_receive_cb_t)(uint8_t *data, fsize_t size);

typedef void (*write_cb_t)(void *ctx, uint8_t *data, fsize_t size);

typedef struct {
  uint8_t id;
  fsize_t size;
  uint8_t data[]; // Flexible array member (must be last)
} __attribute__((packed)) serial_hub_raw_packet;

typedef struct {
  fsize_t max_length;
  fsize_t expected_length;
  int8_t state;
  on_receive_cb_t callback;
} serial_hub_topic_t;

typedef struct {
  int8_t state;
  uint8_t current_topic;

  uint8_t *__read_buf;
  uint8_t *__write_buf;
  uint8_t *__cobsr_tmp_buf;
  fsize_t __read_buf_size;
  fsize_t __write_buf_size;
  fsize_t __cobsr_tmp_buf_size;

  fsize_t __expected_length;
  fsize_t __real_max_length;
  void *__write_cb_context;

  uint16_t __count;
  write_cb_t __write_cb;
  serial_hub_topic_t topics[SERIAL_HUB_TOPIC_MAX];
} serial_hub_handle_t;

#define SERIAL_HUB_ERR_INVALID_SIZE -1
#define SERIAL_HUB_ERR_INVALID_STATE -1
#define SERIAL_HUB_ERR_OCCUPIED -2
#define SERIAL_HUB_ERR_NOT_INITIALIZED -3
#define SERIAL_HUB_ERR_FATAL -4
#define SERIAL_HUB_ERR_OUT_OF_BOUND -5

#define SERIAL_HUB_STATE_INITIALIZED 1
#define SERIAL_HUB_STATE_UNITIALIZED 2
#define SERIAL_HUB_STATE_DEINITIALIZED -1

#define SERIAL_HUB_READ_STATE_HIT_FIRST_ZERO 3
#define SERIAL_HUB_READ_STATE_WAITING_FOR_LENGTH 4
#define SERIAL_HUB_READ_STATE_WAITING_FOR_LENGTH_MSB 5
#define SERIAL_HUB_READ_STATE_READING 6
#define SERIAL_HUB_READ_STATE_WAITING_FOR_ID 8
#define SERIAL_HUB_READ_STATE_EMPTY 7

#define SERIAL_HUB_OK 0

void serial_hub_on_read(serial_hub_handle_t *handle, uint8_t *data,
                        fsize_t size);

int8_t serial_hub_attach_topic(serial_hub_handle_t *handle, uint8_t id,
                               fsize_t size, on_receive_cb_t callback);

int8_t serial_hub_dettach_topic(serial_hub_handle_t *handle, uint8_t id,
                                on_receive_cb_t callback);

int8_t serial_hub_write_topic(serial_hub_handle_t *handle, uint8_t id,
                              uint8_t *data, fsize_t size);

void serial_hub_get_err(char *str, fsize_t size);

int8_t serial_hub_initialize(serial_hub_handle_t *handle, write_cb_t write_cb,
                             void *ctx);

void serial_hub_reserve_memory(serial_hub_handle_t *handle,
                               fsize_t max_message_length);

/*****************************************************************************
 *
 * cobsr.h
 *
 * Consistent Overhead Byte Stuffing--Reduced (COBS/R)
 *
 ****************************************************************************/

/*****************************************************************************
 * Includes
 ****************************************************************************/

#include <stdint.h>
#include <stdlib.h>

/*****************************************************************************
 * Defines
 ****************************************************************************/

#define COBSR_ENCODE_DST_BUF_LEN_MAX(SRC_LEN)                                  \
  (((SRC_LEN) == 0u) ? 1u : ((SRC_LEN) + (((SRC_LEN) + 253u) / 254u)))
#define COBSR_DECODE_DST_BUF_LEN_MAX(SRC_LEN) (SRC_LEN)

/*
 * For in-place encoding, the source data must be offset in the buffer by
 * the following amount (or more).
 */
#define COBSR_ENCODE_SRC_OFFSET(SRC_LEN) (((SRC_LEN) + 253u) / 254u)

/*****************************************************************************
 * Typedefs
 ****************************************************************************/

typedef enum {
  COBSR_ENCODE_OK = 0x00,
  COBSR_ENCODE_NULL_POINTER = 0x01,
  COBSR_ENCODE_OUT_BUFFER_OVERFLOW = 0x02
} cobsr_encode_status;

typedef struct {
  fsize_t out_len;
  cobsr_encode_status status;
} cobsr_encode_result;

typedef enum {
  COBSR_DECODE_OK = 0x00,
  COBSR_DECODE_NULL_POINTER = 0x01,
  COBSR_DECODE_OUT_BUFFER_OVERFLOW = 0x02,
  COBSR_DECODE_ZERO_BYTE_IN_INPUT = 0x04,
} cobsr_decode_status;

typedef struct {
  fsize_t out_len;
  cobsr_decode_status status;
} cobsr_decode_result;

/*****************************************************************************
 * Function prototypes
 ****************************************************************************/

/* COBS/R-encode a string of input bytes, which may save one byte of output.
 *
 * dst_buf_ptr:    The buffer into which the result will be written
 * dst_buf_len:    Length of the buffer into which the result will be written
 * src_ptr:        The byte string to be encoded
 * src_len         Length of the byte string to be encoded
 *
 * returns:        A struct containing the success status of the encoding
 *                 operation and the length of the result (that was written to
 *                 dst_buf_ptr)
 */
cobsr_encode_result cobsr_encode(void *dst_buf_ptr, fsize_t dst_buf_len,
                                 const void *src_ptr, fsize_t src_len);

/* Decode a COBS/R byte string.
 *
 * dst_buf_ptr:    The buffer into which the result will be written
 * dst_buf_len:    Length of the buffer into which the result will be written
 * src_ptr:        The byte string to be decoded
 * src_len         Length of the byte string to be decoded
 *
 * returns:        A struct containing the success status of the decoding
 *                 operation and the length of the result (that was written to
 *                 dst_buf_ptr)
 */
cobsr_decode_result cobsr_decode(void *dst_buf_ptr, fsize_t dst_buf_len,
                                 const void *src_ptr, fsize_t src_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // SERIAL_HUB_H
