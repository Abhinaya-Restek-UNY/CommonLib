
// #define SERIAL_HUB_RESERVE_MEMORY
//
#ifdef __cplusplus
extern "C" {
#endif
#include "serial_hub.h"
#include "stddef.h"
#include "stdlib.h"
#include <stdio.h>

#define SERIAL_HUB_PAYLOAD_SIZE 5

int8_t serial_hub_initialize(serial_hub_handle_t *handle, write_cb_t write_cb,
                             void *ctx) {

  handle->__expected_length = 0;
  handle->__count = 0;
  handle->__read_buf = NULL;
  handle->__write_cb = write_cb;
  handle->__real_max_length = 0;
  handle->__write_cb_context = ctx;
  handle->state = SERIAL_HUB_READ_STATE_EMPTY;
  handle->__write_buf = NULL;
  handle->__write_buf_size = 0;
  handle->__cobsr_tmp_buf = NULL;
  handle->__cobsr_tmp_buf_size = 0;
  handle->__read_buf_size = 0;
  for (uint8_t i = 0; i < SERIAL_HUB_TOPIC_MAX; i++) {
    serial_hub_topic_t *topic = &handle->topics[i];
    topic->callback = NULL;
    topic->state = SERIAL_HUB_STATE_UNITIALIZED;
    topic->max_length = 0;
  }

  return SERIAL_HUB_OK;
};

void serial_hub_reserve_memory(serial_hub_handle_t *handle,
                               fsize_t max_message_length) {
#ifdef SERIAL_HUB_RESERVE_MEMORY

  // WARN: Not sure if this is gonna be efficient (i think it could cause
  // memory fragmentation if this func called too many times);
  if (handle->__write_buf_size) {
    free(handle->__write_buf);
  }

  if (handle->__cobsr_tmp_buf_size) {
    free(handle->__cobsr_tmp_buf);
  }

  if (handle->__read_buf_size) {
    free(handle->__read_buf);
  };

  handle->__write_buf_size = COBSR_ENCODE_DST_BUF_LEN_MAX(max_message_length) +
                             SERIAL_HUB_PAYLOAD_SIZE;

  handle->__read_buf_size = COBSR_ENCODE_DST_BUF_LEN_MAX(max_message_length);
  handle->__cobsr_tmp_buf_size = COBSR_DECODE_DST_BUF_LEN_MAX(
      COBSR_ENCODE_DST_BUF_LEN_MAX(max_message_length));

  handle->__cobsr_tmp_buf = (uint8_t *)malloc(handle->__cobsr_tmp_buf_size);

  handle->__write_buf = (uint8_t *)malloc(handle->__write_buf_size);

  handle->__read_buf = (uint8_t *)malloc(handle->__read_buf_size);

#endif
};

int8_t serial_hub_attach_topic(serial_hub_handle_t *handle, uint8_t id,
                               fsize_t size, on_receive_cb_t callback) {

  fsize_t max_len = COBSR_ENCODE_DST_BUF_LEN_MAX(size);

  if (handle->__real_max_length < max_len) {
    handle->__real_max_length = max_len;
  }

  if (id >= SERIAL_HUB_TOPIC_MAX) {

    return SERIAL_HUB_ERR_OUT_OF_BOUND;
  }

  serial_hub_topic_t *topic = &(handle->topics[id]);

  if (topic->state == SERIAL_HUB_STATE_INITIALIZED) {
    return SERIAL_HUB_ERR_OCCUPIED;
  }

  topic->state = SERIAL_HUB_STATE_INITIALIZED;

  topic->callback = callback;
  topic->max_length = max_len;
  topic->expected_length = size;

  return SERIAL_HUB_OK;
};

int8_t serial_hub_dettach_topic(serial_hub_handle_t *handle, uint8_t id,
                                on_receive_cb_t callback) {

  if (id >= SERIAL_HUB_TOPIC_MAX) {
    return SERIAL_HUB_ERR_OUT_OF_BOUND;
  }

  serial_hub_topic_t *topic = &(handle->topics[id]);
  if (topic->state != SERIAL_HUB_STATE_INITIALIZED) {
    return SERIAL_HUB_ERR_INVALID_STATE;
  }

  topic->callback = NULL;
  topic->state = SERIAL_HUB_STATE_UNITIALIZED;
  topic->max_length = 0;

  return SERIAL_HUB_OK;
}

void serial_hub_get_err(char *str, fsize_t size) {};

int8_t serial_hub_write_topic(serial_hub_handle_t *handle, uint8_t id,
                              uint8_t *data, fsize_t size) {

  fsize_t encoded_max_size = COBSR_ENCODE_DST_BUF_LEN_MAX(size);

#ifndef SERIAL_HUB_RESERVE_MEMORY
  handle->__write_buf =
      (uint8_t *)malloc(encoded_max_size + SERIAL_HUB_PAYLOAD_SIZE);
  handle->__write_buf_size = encoded_max_size + SERIAL_HUB_PAYLOAD_SIZE;
#endif

  // [uint16_t(0) start_byte][uint16_t expected_length][...data]
  fsize_t cobsr_result_len =
      cobsr_encode(&handle->__write_buf[SERIAL_HUB_PAYLOAD_SIZE],
                   handle->__write_buf_size, data, size)
          .out_len;

  handle->__write_buf[0] = 0;
  handle->__write_buf[1] = 0;
  handle->__write_buf[2] =
      (uint8_t)(cobsr_result_len & 0xFF); // Least significant byte
  handle->__write_buf[3] =
      (uint8_t)((cobsr_result_len >> 8) & 0xFF); // Most significant byte
  handle->__write_buf[4] = id;

  handle->__write_cb(handle->__write_cb_context, handle->__write_buf,
                     cobsr_result_len + SERIAL_HUB_PAYLOAD_SIZE);

#ifndef SERIAL_HUB_RESERVE_MEMORY
  free(handle->__write_buf);
  handle->__write_buf_size = 0;
#endif

  return SERIAL_HUB_OK;
};

void serial_hub_reading_finished(serial_hub_handle_t *handle) {
#ifndef SERIAL_HUB_RESERVE_MEMORY
  handle->__cobsr_tmp_buf_size =
      COBSR_DECODE_DST_BUF_LEN_MAX(handle->__expected_length);
  handle->__cobsr_tmp_buf = (uint8_t *)malloc(handle->__cobsr_tmp_buf_size);
#endif

  fsize_t decoded_len =
      cobsr_decode(handle->__cobsr_tmp_buf, handle->__cobsr_tmp_buf_size,
                   handle->__read_buf, handle->__expected_length)
          .out_len;
  //
  if (decoded_len == handle->topics[handle->current_topic].expected_length) {

    handle->topics[handle->current_topic].callback(handle->__cobsr_tmp_buf,
                                                   decoded_len);
  }

  handle->state = SERIAL_HUB_READ_STATE_EMPTY;
  handle->__expected_length = 0;
  handle->__count = 0;

#ifndef SERIAL_HUB_RESERVE_MEMORY
  free(handle->__cobsr_tmp_buf);
  handle->__cobsr_tmp_buf_size = 0;
  free(handle->__read_buf);
  handle->__read_buf_size = 0;

#endif
};

void serial_hub_start_reading(serial_hub_handle_t *handle, uint8_t topic_id) {
  if (topic_id >= SERIAL_HUB_TOPIC_MAX) {
    handle->state = SERIAL_HUB_READ_STATE_EMPTY;
    return;
  }

  if (handle->topics[topic_id].state != SERIAL_HUB_STATE_INITIALIZED) {
    handle->state = SERIAL_HUB_READ_STATE_EMPTY;
    return;
  }

  if (handle->topics[topic_id].max_length < handle->__expected_length) {
    handle->state = SERIAL_HUB_READ_STATE_EMPTY;
    return;
  }

  handle->state = SERIAL_HUB_READ_STATE_READING;
  handle->current_topic = topic_id;

#ifndef SERIAL_HUB_RESERVE_MEMORY
  handle->__read_buf = (uint8_t *)malloc(handle->__expected_length);
  handle->__read_buf_size = handle->__expected_length;
#endif
};

void serial_hub_on_read(serial_hub_handle_t *handle, uint8_t *data,
                        fsize_t size) {

  for (fsize_t i = 0; i < size; i++) {
    uint8_t byte = data[i];
    if (!byte) {
      if (handle->state == SERIAL_HUB_READ_STATE_HIT_FIRST_ZERO) {

        handle->state = SERIAL_HUB_READ_STATE_WAITING_FOR_LENGTH;
        continue;
      } else if (handle->state == SERIAL_HUB_READ_STATE_EMPTY) {
        handle->state = SERIAL_HUB_READ_STATE_HIT_FIRST_ZERO;

        if (handle->__expected_length) {

#ifndef SERIAL_HUB_RESERVE_MEMORY
          free(handle->__read_buf);
          handle->__read_buf_size = 0;
#endif
          handle->__count = 0;
          handle->__expected_length = 0;
        }
        continue;
      }
    }

    switch (handle->state) {
    case SERIAL_HUB_READ_STATE_WAITING_FOR_LENGTH:

      handle->state = SERIAL_HUB_READ_STATE_WAITING_FOR_LENGTH_MSB;
      handle->__expected_length = byte;
      break;
    case SERIAL_HUB_READ_STATE_WAITING_FOR_LENGTH_MSB:

      handle->__expected_length =
          handle->__expected_length | ((fsize_t)byte << 8);
      // FIXED: Added this back!
      handle->state = SERIAL_HUB_READ_STATE_WAITING_FOR_ID;
      break;

    case SERIAL_HUB_READ_STATE_WAITING_FOR_ID:
      serial_hub_start_reading(handle, byte);
      break;

    case SERIAL_HUB_READ_STATE_READING:

      if (handle->__count > handle->__expected_length) {

#ifndef SERIAL_HUB_RESERVE_MEMORY
        free(handle->__read_buf);
        handle->__read_buf_size = 0;
#endif
        handle->__count = 0;
        handle->__expected_length = 0;
        // FIXED: Failsafe reset placed correctly!
        handle->state = SERIAL_HUB_READ_STATE_EMPTY;
      } else {
        handle->__read_buf[handle->__count] = byte;
        handle->__count++;
        if (handle->__count == handle->__expected_length) {
          serial_hub_reading_finished(handle);
        }
      }
      break;
    default:
      break;
    }
  }
}

void serial_hub_destroy(serial_hub_handle_t *handle) {
  if (handle->__read_buf_size) {
    free(handle->__read_buf);
  }

  if (handle->__cobsr_tmp_buf_size) {
    free(handle->__cobsr_tmp_buf);
  }

  if (handle->__write_buf_size) {
    free(handle->__write_buf);
  }
}

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
                                 const void *src_ptr, fsize_t src_len) {
  cobsr_encode_result result = {0u, COBSR_ENCODE_OK};
  const uint8_t *src_read_ptr = (uint8_t *)src_ptr;
  const uint8_t *src_end_ptr = (uint8_t *)src_ptr + src_len;
  uint8_t *dst_buf_start_ptr = (uint8_t *)dst_buf_ptr;
  uint8_t *dst_buf_end_ptr = (uint8_t *)dst_buf_ptr + dst_buf_len;
  uint8_t *dst_code_write_ptr = (uint8_t *)dst_buf_ptr;
  uint8_t *dst_write_ptr = dst_code_write_ptr + 1u;
  uint8_t src_byte = 0u;
  uint8_t search_len = 1u;

  /* First, do a NULL pointer check and return immediately if it fails. */
  if ((dst_buf_ptr == NULL) || (src_ptr == NULL)) {
    result.status = COBSR_ENCODE_NULL_POINTER;
    return result;
  }

  if (src_len != 0u) {
    /* Iterate over the source bytes */
    for (;;) {
      /* Check for running out of output buffer space */
      if (dst_write_ptr >= dst_buf_end_ptr) {
        result.status = (cobsr_encode_status)(result.status |
                                              COBSR_ENCODE_OUT_BUFFER_OVERFLOW);
        break;
      }

      src_byte = *src_read_ptr++;
      if (src_byte == 0u) {
        /* We found a zero byte */
        *dst_code_write_ptr = search_len;
        dst_code_write_ptr = dst_write_ptr++;
        search_len = 1u;
        if (src_read_ptr >= src_end_ptr) {
          break;
        }
      } else {
        /* Copy the non-zero byte to the destination buffer */
        *dst_write_ptr++ = src_byte;
        search_len++;
        if (src_read_ptr >= src_end_ptr) {
          break;
        }
        if (search_len == 0xFF) {
          /* We have a long string of non-zero bytes, so we need
           * to write out a length code of 0xFF. */
          *dst_code_write_ptr = search_len;
          dst_code_write_ptr = dst_write_ptr++;
          search_len = 1u;
        }
      }
    }
  }

  /* We've reached the end of the source data (or possibly run out of output
   * buffer) Finalise the remaining output. In particular, write the code
   * (length) byte.
   *
   * For COBS/R, the final code (length) byte is special: if the final data byte
   * is greater than or equal to what would normally be the final code (length)
   * byte, then replace the final code byte with the final data byte, and remove
   * the final data byte from the end of the sequence. This saves one byte in
   * the output.
   *
   * Update the pointer to calculate the final output length.
   */
  if (dst_code_write_ptr >= dst_buf_end_ptr) {
    /* We've run out of output buffer to write the code byte. */
    result.status =
        (cobsr_encode_status)(result.status | COBSR_ENCODE_OUT_BUFFER_OVERFLOW);
    dst_write_ptr = dst_buf_end_ptr;
  } else {
    if (src_byte < search_len) {
      /* Encoding same as plain COBS */
      *dst_code_write_ptr = search_len;
    } else {
      /* Special COBS/R encoding: length code is final byte,
       * and final byte is removed from data sequence. */
      *dst_code_write_ptr = src_byte;
      dst_write_ptr--;
    }
  }

  /* Calculate the output length, from the value of dst_code_write_ptr */
  result.out_len = (fsize_t)(dst_write_ptr - dst_buf_start_ptr);

  return result;
}

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
                                 const void *src_ptr, fsize_t src_len) {
  cobsr_decode_result result = {0u, COBSR_DECODE_OK};
  const uint8_t *src_read_ptr = (uint8_t *)src_ptr;
  const uint8_t *src_end_ptr = (uint8_t *)src_ptr + src_len;
  uint8_t *dst_buf_start_ptr = (uint8_t *)dst_buf_ptr;
  uint8_t *dst_buf_end_ptr = (uint8_t *)dst_buf_ptr + dst_buf_len;
  uint8_t *dst_write_ptr = (uint8_t *)dst_buf_ptr;
  fsize_t remaining_input_bytes;
  fsize_t remaining_output_bytes;
  uint8_t num_output_bytes;
  uint8_t src_byte;
  uint8_t i;
  uint8_t len_code;

  /* First, do a NULL pointer check and return immediately if it fails. */
  if ((dst_buf_ptr == NULL) || (src_ptr == NULL)) {
    result.status = COBSR_DECODE_NULL_POINTER;
    return result;
  }

  if (src_len != 0u) {
    for (;;) {
      len_code = *src_read_ptr++;
      if (len_code == 0u) {
        result.status = (cobsr_decode_status)(result.status |
                                              COBSR_DECODE_ZERO_BYTE_IN_INPUT);
        break;
      }

      /* Calculate remaining input bytes */
      remaining_input_bytes = (fsize_t)(src_end_ptr - src_read_ptr);

      if ((len_code - 1u) < remaining_input_bytes) {
        num_output_bytes = (uint8_t)(len_code - 1u);

        /* Check length code against remaining output buffer space */
        remaining_output_bytes = (fsize_t)(dst_buf_end_ptr - dst_write_ptr);
        if (num_output_bytes > remaining_output_bytes) {
          result.status =
              (cobsr_decode_status)(result.status |
                                    COBSR_DECODE_OUT_BUFFER_OVERFLOW);
          num_output_bytes = (uint8_t)remaining_output_bytes;
        }

        for (i = num_output_bytes; i != 0u; i--) {
          src_byte = *src_read_ptr++;
          if (src_byte == 0u) {
            result.status =
                (cobsr_decode_status)(result.status |
                                      COBSR_DECODE_ZERO_BYTE_IN_INPUT);
          }
          *dst_write_ptr++ = src_byte;
        }

        /* Add a zero to the end */
        if (len_code != 0xFFu) {
          if (dst_write_ptr >= dst_buf_end_ptr) {
            result.status =
                (cobsr_decode_status)(result.status |
                                      COBSR_DECODE_OUT_BUFFER_OVERFLOW);
            break;
          }
          *dst_write_ptr++ = '\0';
        }
      } else {
        /* We've reached the last length code, so write the remaining
         * bytes and then exit the loop. */

        num_output_bytes = (uint8_t)remaining_input_bytes;

        /* Check length code against remaining output buffer space */
        remaining_output_bytes = (fsize_t)(dst_buf_end_ptr - dst_write_ptr);
        if (num_output_bytes > remaining_output_bytes) {
          result.status =
              (cobsr_decode_status)(result.status |
                                    COBSR_DECODE_OUT_BUFFER_OVERFLOW);
          num_output_bytes = (uint8_t)remaining_output_bytes;
        }

        for (i = num_output_bytes; i != 0u; i--) {
          src_byte = *src_read_ptr++;
          if (src_byte == 0u) {
            result.status =
                (cobsr_decode_status)(result.status |
                                      COBSR_DECODE_ZERO_BYTE_IN_INPUT);
          }
          *dst_write_ptr++ = src_byte;
        }

        /* Write final data byte, if applicable for COBS/R encoding. */
        if ((len_code - 1u) > remaining_input_bytes) {
          if (dst_write_ptr >= dst_buf_end_ptr) {
            result.status =
                (cobsr_decode_status)(result.status |
                                      COBSR_DECODE_OUT_BUFFER_OVERFLOW);
          } else {
            *dst_write_ptr++ = len_code;
          }
        }

        /* Exit the loop */
        break;
      }
    }

    result.out_len = (fsize_t)(dst_write_ptr - dst_buf_start_ptr);
  }

  return result;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
