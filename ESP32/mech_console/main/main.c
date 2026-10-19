/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_health.h"
#include "app_uart.h"

#define UNUSED(x) (void)(x)

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP");

/* Test the app health task by attempting to start it twice */
#define APP_HEALTH_START_TEST
#undef APP_HEALTH_START_TEST

#define APP_HEALTH_STOP_TEST
#undef APP_HEALTH_STOP_TEST

static void print_chip_info(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
            (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
            (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(TAG, "silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }

    ESP_LOGI(TAG, "%" PRIu32 "MB %s flash", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
}

#ifdef APP_HEALTH_STOP_TEST
static void timer_stop_callback(void* arg)
{
    esp_err_t err = app_health_request_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to request stop for app health task: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Successfully requested stop for app health task");
    }

    /* Notify the main task that the timer has triggered */
    (void) xTaskNotifyGive((TaskHandle_t)arg);
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "Hello world!\n");

    /* Print chip information */
    print_chip_info();

    esp_err_t err = ESP_OK;

#ifdef APP_HEALTH_STOP_TEST
    /* Create a one-shot timer to stop the app health task 5500ms after it starts */
    esp_timer_create_args_t timer_args = {
        .callback = timer_stop_callback,
        .dispatch_method = ESP_TIMER_TASK,
        /* Pass the handle of the current task to the timer callback */
        .arg = xTaskGetCurrentTaskHandle(),
        .name = "app_health_stop_timer"
    };
    esp_timer_handle_t timer;
    err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create app health stop timer: %s", esp_err_to_name(err));
        return;
    }
#endif

    /* Start the app health task */
    err = app_health_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app health task: %s", esp_err_to_name(err));
#ifdef APP_HEALTH_STOP_TEST
        /* Delete the timer if starting the app health task failed */
        esp_timer_delete(timer);
        return;
#endif
    }

#ifdef APP_HEALTH_STOP_TEST
    /* Start the one-shot timer to stop the app health task after 5500ms */
    err = esp_timer_start_once(timer, 5500 * 1000); // 5500ms in microseconds
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app health stop timer: %s", esp_err_to_name(err));
        /* Delete the timer if starting it failed */
        esp_timer_delete(timer);
        return;
    }
#endif

#ifdef APP_HEALTH_START_TEST
    /* Call the app_health_start again to test the ESP_ERR_INVALID_STATE return value */
    err = app_health_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app health task again: %s", esp_err_to_name(err));
    }
#endif

#ifdef APP_HEALTH_STOP_TEST
    uint32_t notify_value = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (notify_value > 0) {
        err = esp_timer_delete(timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete app health stop timer: %s", esp_err_to_name(err));
        }
    }
#endif

    err = app_uart_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app UART: %s", esp_err_to_name(err));
        return;
    }

}
