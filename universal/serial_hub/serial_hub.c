
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

  handle->__read_buf_size = max_message_length + max_message_length / 255 + 2;
  handle->__write_buf_size = handle->__read_buf_size + SERIAL_HUB_PAYLOAD_SIZE;

  handle->__write_buf = (uint8_t *)malloc(handle->__write_buf_size);

  handle->__read_buf = (uint8_t *)malloc(handle->__read_buf_size);
};

int8_t serial_hub_attach_topic(serial_hub_handle_t *handle, uint8_t id,
                               fsize_t size, void *ctx,
                               on_receive_cb_t callback) {

  fsize_t max_len = size + (size / 255) + 2;

  if (id >= SERIAL_HUB_TOPIC_MAX) {

    return SERIAL_HUB_ERR_OUT_OF_BOUND;
  }

  serial_hub_topic_t *topic = &(handle->topics[id]);

  if (topic->state == SERIAL_HUB_STATE_INITIALIZED) {
    return SERIAL_HUB_ERR_OCCUPIED;
  }

  topic->state = SERIAL_HUB_STATE_INITIALIZED;

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
inline fsize_t serial_hub_start_reading(serial_hub_handle_t *handle,
                                        uint8_t *start, uint8_t *end) {

  fsize_t count_start = handle->__count;

  for (uint8_t *byte = start;
       byte < end &&
       handle->__count < handle->__current_topic->expected_length;) {

    uint8_t len = MIN(end - byte, handle->__next_zero - handle->__count + 1);

    if (len < 1) {
      break;
    }

    memcpy(handle->__read_buf + handle->__count, byte, len);

    handle->__read_buf[handle->__next_zero] = 0xFF;
    if (start + handle->__next_zero - count_start < end) {
      handle->__next_zero += *(start + handle->__next_zero - count_start);
    }

    handle->__count += len;
    byte += len;
  }

  fsize_t total_processed = handle->__count - count_start;

  if (handle->__count == handle->__current_topic->expected_length) {
    handle->__current_topic->callback(handle->__current_topic->ctx,
                                      handle->__read_buf, handle->__count);
    handle->__count = 0;
    handle->__current_topic = NULL;
    handle->__next_zero = 0;
  }
  return total_processed;
};

void serial_hub_on_read(serial_hub_handle_t *handle, uint8_t *data,
                        fsize_t size) {

  uint8_t *end_of_data = data + size;

  for (uint8_t *byte = data; byte < end_of_data; byte++) {

    if (*byte == 0) {
      handle->__count = 0;
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
        byte += serial_hub_get_next_packet(byte, end_of_data);
      }
      break;
    case SERIAL_HUB_READ_STATE_HIT_ID:

      handle->__next_zero = *byte;
      handle->state = SERIAL_HUB_READ_STATE_HIT_READING;

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
