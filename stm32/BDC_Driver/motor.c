#include "motor.h"

void motor_init(motor_handle_t *context) {
  HAL_TIM_PWM_Start(context->htim, context->channel);
};

void motor_set_direction(motor_handle_t *context, int16_t direction) {
  if (direction == 0) {
    __HAL_TIM_SET_COMPARE(context->htim, context->channel, 0);
    HAL_GPIO_WritePin(context->GPIOx_in1, context->pin_in1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(context->GPIOx_in2, context->pin_in2, GPIO_PIN_RESET);

    return;
  }

  uint16_t speed = abs(direction);

  __HAL_TIM_SET_COMPARE(context->htim, context->channel, speed >> 2);

  if (direction > 0) {
    HAL_GPIO_WritePin(context->GPIOx_in1, context->pin_in1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(context->GPIOx_in2, context->pin_in2, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(context->GPIOx_in1, context->pin_in1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(context->GPIOx_in2, context->pin_in2, GPIO_PIN_SET);
  }
};
