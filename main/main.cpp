#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>

#include "driver/uart.h"
#include "custom_networking.h"
#include "esp32_serial_transport.h"

#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>


static const char *TAG = "main";
static size_t uart_port = UART_NUM_0;

/* Set USE_ROS_EXECUTOR to 1 if we need a microROS executor running.
   This would be if we want to use ROS timers, or subscriptions. */
#define USE_ROS_EXECUTOR 0

#define RCCHECK(fn)                                                                \
  {                                                                                \
    rcl_ret_t temp_rc = fn;                                                        \
    if ((temp_rc != RCL_RET_OK))                                                   \
    {                                                                              \
      printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); \
      vTaskDelete(NULL);                                                           \
    }                                                                              \
  }
#define RCSOFTCHECK(fn)                                                              \
  {                                                                                  \
    rcl_ret_t temp_rc = fn;                                                          \
    if ((temp_rc != RCL_RET_OK))                                                     \
    {                                                                                \
      printf("Failed status on line %d: %d. Continuing.\n", __LINE__, (int)temp_rc); \
    }                                                                                \
  }

extern "C"
{
  void app_main(void);
}

void app_main(void)
{

	rmw_uros_set_custom_transport(
		true,
		(void *) &uart_port,
		esp32_serial_open,
		esp32_serial_close,
		esp32_serial_write,
		esp32_serial_read
	);

  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;

  bool microros_connection_verified = false;

  // Verify connection to agent before proceeding
  while (!microros_connection_verified)
  {
    rmw_ret_t ping_result = rmw_uros_ping_agent(1000, 1);
    if (RMW_RET_OK == ping_result) {
      ESP_LOGI(TAG, "Connection to microROS agent verified");
      microros_connection_verified = true;
    } else {
      ESP_LOGI(TAG, "Connection to microROS agent failed, will retry...");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // create init_options 
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // create node
  rcl_node_t node;
  RCCHECK(rclc_node_init_default(&node, "foobar", "", &support));

  rmw_ret_t sync_result = rmw_uros_sync_session(1000);
  if (sync_result == RMW_RET_OK)
  {
    ESP_LOGI(TAG, "Initial time synchronized with microROS agent");
  }

  int counter = 0;
  int has_failed = 0;
  int sync_interval_ms = 500;
  while (true)
  {
    vTaskDelay(sync_interval_ms);

    counter++;
    int64_t pre_nanos = rmw_uros_epoch_nanos();
    sync_result = rmw_uros_sync_session(3);
    if (sync_result != RMW_RET_OK)
    {
      ESP_LOGI(TAG, "total fails: %i iteration: %d Time sync failed", has_failed, counter);
    } else {
      ESP_LOGI(TAG, "total fails: %i iteration: %d Time sync succeeded", has_failed, counter);
    }
    int64_t post_nanos = rmw_uros_epoch_nanos();

    // Look for the issue. Usually we would just back 0.5 times the sync_interval_ms, but since the calls takes
    // some times etc we use 0.3 to check.
    if ((pre_nanos-post_nanos) > sync_interval_ms*1e6*0.3)
    {
      has_failed++;
      ESP_LOGI(TAG, "iteration: %d Time sync bug triggered post: %lli pre: %lli", counter, post_nanos, pre_nanos);
    }
  }
  RCCHECK(rcl_node_fini(&node));
}