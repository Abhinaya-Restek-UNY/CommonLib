
#include "MPU6050.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include <driver/i2c.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define MPU_INT_PIN GPIO_NUM_2

static TaskHandle_t mpuTaskHandle = NULL;

// 1. THE INTERRUPT HANDLER (Keep this FAST!)
// IRAM_ATTR forces this code into RAM so it runs super fast.
static void IRAM_ATTR gpio_isr_handler(void *arg) {

  // Notify the task that an interrupt happened.
  // We use "FromISR" version because we are inside an interrupt.
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(mpuTaskHandle, &xHigherPriorityTaskWoken);

  // If the task we woke up is high priority, switch to it immediately.
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
VectorInt16 accel_raw;

uint32_t prev;

#define resol 9.81 / 16834.0

VectorInt16 accel;
Quaternion q;        // [w, x, y, z]         quaternion container
VectorFloat gravity; // [x, y, z]            gravity vector
float
    ypr[3]; // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
uint16_t packetSize = 42; // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;       // count of all bytes currently in FIFO
uint8_t fifoBuffer[64];   // FIFO storage buffer
uint8_t mpuIntStatus;     // holds actual interrupt status byte from MPU
void mpu_task(void *pvParameter) {
  MPU6050 *mpu = (MPU6050 *)pvParameter;

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // read a packet from FIFO
    mpu->dmpGetCurrentFIFOPacket(fifoBuffer);
    // display Euler angles in degrees
    mpu->dmpGetQuaternion(&q, fifoBuffer);
    mpu->dmpGetGravity(&gravity, &q);
    mpu->dmpGetYawPitchRoll(ypr, &q, &gravity);
    printf("ypr\t");
    printf("%f", ypr[0] * 180 / M_PI);
    printf("\t");
    printf("%f", ypr[1] * 180 / M_PI);
    printf("\t");
    printf("%f\n", ypr[2] * 180 / M_PI);
  }
}

//
void setup_interrupt(MPU6050 *mpu) {

  gpio_config_t io_conf{
      .pin_bit_mask = (1ULL << MPU_INT_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en =
          GPIO_PULLUP_DISABLE, // MPU6050 INT is usually Push-Pull active high
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_POSEDGE, // Trigger on Rising Edge (0 -> 1)
  };

  xTaskCreate(mpu_task, "mpu_task", 4096, mpu, 10, &mpuTaskHandle);

  gpio_config(&io_conf);
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  gpio_isr_handler_add(MPU_INT_PIN, gpio_isr_handler, NULL);
}

extern "C" void app_main(void) {

  i2c_master_bus_handle_t bus_hande;
  i2c_master_bus_config_t bus_conf = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = GPIO_NUM_6,
      .scl_io_num = GPIO_NUM_7,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags = {.enable_internal_pullup = true}};

  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_conf, &bus_hande));
  vTaskDelay(pdMS_TO_TICKS(100));
  MPU6050 mpu = MPU6050(bus_hande);

  vTaskDelay(pdMS_TO_TICKS(100));

  printf("Setting up mpu\n");
  mpu.initialize();

  printf("Setting up dmp\n");
  uint8_t devStatus = mpu.dmpInitialize();
  mpu.setXAccelOffset(-2499);
  mpu.setYAccelOffset(1065);
  mpu.setZAccelOffset(1681);
  mpu.setXGyroOffset(33);
  mpu.setYGyroOffset(-2);
  mpu.setZGyroOffset(-17);
  if (devStatus == 0) {

    printf("Calibrating...\n");
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.PrintActiveOffsets();
    printf("Successfully setup dmp bro\n enabling dmp\n");

    mpu.setDMPEnabled(true);

    mpuIntStatus = mpu.getIntStatus();
    printf("int status(?) 0x%x\n", mpuIntStatus);

    packetSize = mpu.dmpGetFIFOPacketSize();

    setup_interrupt(&mpu);
  } else {
    printf("Failed to setup dmp.\n");
  }

  while (1) {
    vTaskDelay(100);
  }
}
