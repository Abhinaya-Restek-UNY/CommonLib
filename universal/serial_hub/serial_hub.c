
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "serial_hub.h"
#include "stddef.h"
#include "stdlib.h"

#define SERIAL_HUB_PAYLOAD_SIZE 2

int8_t serial_hub_initialize(serial_hub_handle_t *handle, write_cb_t write_cb,
                             void *ctx) {

  handle->state = SERIAL_HUB_READ_STATE_EMPTY;

  handle->__count = 0;
  handle->__read_buf = NULL;
  handle->__write_cb = write_cb;
  handle->__write_cb_context = ctx;
  handle->__next_zero = 0;
  handle->__write_buf = NULL;
  handle->__write_buf_size = 0;
  handle->__read_buf_size = 0;
  handle->__current_topic = NULL;

  for (uint8_t i = 0; i < SERIAL_HUB_TOPIC_MAX; i++) {
    serial_hub_topic_t *topic = &handle->topics[i];
    topic->callback = NULL;
    topic->state = SERIAL_HUB_STATE_UNITIALIZED;
    topic->expected_length = 0;
    topic->ctx = 0;
  }

  return SERIAL_HUB_OK;
};

void serial_hub_reserve_memory(serial_hub_handle_t *handle,
                               fsize_t max_message_length) {
  // WARN: Not sure if this is gonna be efficient (i think it could cause
  // memory fragmentation if this func called too many times);
  if (handle->__write_buf_size) {
    free(handle->__write_buf);
  }

  if (handle->__read_buf_size) {
    free(handle->__read_buf);
  };

  handle->__read_buf_size = COBS_ENCODE_DST_BUF_LEN_MAX(max_message_length);
  handle->__write_buf_size = handle->__read_buf_size + SERIAL_HUB_PAYLOAD_SIZE;

  handle->__write_buf = (uint8_t *)malloc(handle->__write_buf_size);

  handle->__read_buf = (uint8_t *)malloc(handle->__read_buf_size);
};

int8_t serial_hub_attach_topic(serial_hub_handle_t *handle, uint8_t id,
                               fsize_t size, void *ctx,
                               on_receive_cb_t callback) {

  if (id >= SERIAL_HUB_TOPIC_MAX) {
    return SERIAL_HUB_ERR_OUT_OF_BOUND;
  }

  serial_hub_topic_t *topic = &(handle->topics[id]);

  if (topic->state == SERIAL_HUB_STATE_INITIALIZED) {
    return SERIAL_HUB_ERR_OCCUPIED;
  }

  topic->state = SERIAL_HUB_STATE_INITIALIZED;
  topic->max_expected_length = COBS_ENCODE_DST_BUF_LEN_MAX(size);

  topic->callback = callback;
  topic->expected_length = size;
  topic->ctx = ctx;

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

  return SERIAL_HUB_OK;
}

void serial_hub_get_err(char *str, fsize_t size) {};

int8_t serial_hub_write_topic(serial_hub_handle_t *handle, uint8_t id,
                              uint8_t *data, fsize_t size) {

  handle->__write_buf[0] = 0;

  handle->__write_buf[1] = id;
  uint8_t *last_zero = &(handle->__write_buf[2]);

  *last_zero = 1;
  fsize_t beginning_idx = 0;
  uint8_t len = 0;
  for (fsize_t i = 0; i < size; i++) {
    if (data[i] == 0) {
      len = i - beginning_idx;
      memcpy(last_zero + 1, &data[beginning_idx], len);
      last_zero += len + 1;
      beginning_idx = i + 1;
      *last_zero = 1;

      continue;
    } else if (*last_zero == 0xff) {
      len = i - beginning_idx;
      memcpy(last_zero + 1, &data[beginning_idx], len);
      beginning_idx = i;
      last_zero += len + 1;
      *last_zero = 1;
    };

    *last_zero += 1;
  };

  memcpy(last_zero + 1, &data[beginning_idx], size - beginning_idx);

  fsize_t total_length =
      (last_zero) - &handle->__write_buf[0] + size - beginning_idx + 1;

  handle->__write_cb(handle->__write_cb_context, handle->__write_buf,
                     total_length);

  return SERIAL_HUB_OK;
};

inline fsize_t serial_hub_get_next_packet(uint8_t *currentCOB_byte,
                                          uint8_t *end) {

  for (uint8_t *byte = currentCOB_byte; byte < end; byte += *byte) {
    if (!(*byte)) {
      return end - byte;
    }
  }

  return end - currentCOB_byte - 1;
};

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

inline fsize_t serial_hub_start_reading(serial_hub_handle_t *handle,
                                        uint8_t *start, uint8_t *end) {
  fsize_t in_idx = 0;
  fsize_t in_len = end - start;

  while (in_idx < in_len &&
         handle->__count < handle->__current_topic->expected_length) {

    // 1. Did we reach the target jump index?
    if (handle->__count == handle->__next_zero) {
      // Extract the jump code we just copied into the buffer during the last
      // memcpy
      uint8_t next_jump = handle->__read_buf[handle->__count - 1];

      if (next_jump == 0) { // Catch invalid/corrupted COBS data
        handle->state = SERIAL_HUB_READ_STATE_EMPTY;
        return in_idx;
      }

      // 2. Check the jump code that BROUGHT us here (stored in __prev_zero)
      if (handle->__prev_zero == 255) {
        // It was a 254-byte max run. Drop the overhead byte by walking back the
        // output counter! We DO NOT walk back in_idx, which natively drops the
        // byte from the stream.
        handle->__count--;
      } else {
        // Normal run. Restore the original 0x00 zero.
        handle->__read_buf[handle->__count - 1] = 0x00;
      }

      // 3. Setup the target for the next jump
      handle->__prev_zero =
          next_jump; // Save this jump code for the next evaluation
      handle->__next_zero =
          handle->__count + next_jump; // Set absolute output target

      continue; // Re-evaluate the loop limits immediately
    }

    // 4. Calculate safe block copy limits
    fsize_t space_to_jump = handle->__next_zero - handle->__count;
    fsize_t space_in_input = in_len - in_idx;
    fsize_t space_in_payload =
        handle->__current_topic->expected_length - handle->__count;

    fsize_t len = MIN(space_to_jump, space_in_input);
    len = MIN(len, space_in_payload);

    if (len > 0) {
      // Use in_idx for the source offset, and __count for the destination
      // offset
      memcpy(handle->__read_buf + handle->__count, start + in_idx, len);
      handle->__count += len;
      in_idx += len;
    }
  }

  // 5. Fire callback if payload is fully assembled
  if (handle->__count == handle->__current_topic->expected_length) {
    handle->__current_topic->callback(handle->__current_topic->ctx,
                                      handle->__read_buf,
                                      handle->__current_topic->expected_length);
    handle->state = SERIAL_HUB_READ_STATE_EMPTY;
  }

  // Return exactly how many bytes we consumed from this chunk
  return in_idx;
};

void serial_hub_on_read(serial_hub_handle_t *handle, uint8_t *data,
                        fsize_t size) {

  uint8_t *end_of_data = data + size;

  for (uint8_t *byte = data; byte < end_of_data; byte++) {

    if (*byte == 0) {
      handle->state = SERIAL_HUB_READ_STATE_HIT_ZERO;
      continue;
    }

    switch (handle->state) {
    case SERIAL_HUB_READ_STATE_HIT_ZERO:
      if (*byte < SERIAL_HUB_TOPIC_MAX &&
          handle->topics[*byte].state == SERIAL_HUB_STATE_INITIALIZED) {
        handle->__current_topic = &handle->topics[*byte];
        handle->state = SERIAL_HUB_READ_STATE_HIT_ID;
      } else {
        handle->state = SERIAL_HUB_READ_STATE_EMPTY;
      }
      break;
    case SERIAL_HUB_READ_STATE_HIT_ID:
      handle->__next_zero = *byte;

      // Prevent 0-byte jump infinite loops
      if (handle->__next_zero > 0 &&
          handle->__next_zero < handle->__current_topic->max_expected_length) {
        handle->__prev_zero = *byte; // Store the actual jump code here!
        handle->state = SERIAL_HUB_READ_STATE_HIT_READING;
        handle->__count = 0;
      } else {
        handle->state = SERIAL_HUB_READ_STATE_EMPTY;
      }
      break;
    case SERIAL_HUB_READ_STATE_HIT_READING:
      byte += serial_hub_start_reading(handle, byte, end_of_data) - 1;
      break;
    case SERIAL_HUB_READ_STATE_EMPTY:
      break;
    }
  }
}

void serial_hub_destroy(serial_hub_handle_t *handle) {
  if (handle->__read_buf_size) {
    free(handle->__read_buf);
  }

  if (handle->__write_buf_size) {
    free(handle->__write_buf);
  }
}

#ifdef __cplusplus
} /* extern "C" */
#endif
