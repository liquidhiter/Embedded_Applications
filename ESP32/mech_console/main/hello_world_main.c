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

#include "app_health.h"

#define UNUSED(x) (void)(x)

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP");

/* Test the app health task by attempting to start it twice */
#define APP_HEALTH_TEST
#undef APP_HEALTH_TEST

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

void app_main(void)
{
    ESP_LOGI(TAG, "Hello world!\n");

    /* Print chip information */
    print_chip_info();

    /* Start the app health task */
    esp_err_t err = app_health_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app health task: %s", esp_err_to_name(err));
    }

#ifdef APP_HEALTH_TEST
    /* Call the app_health_start again to test the ESP_ERR_INVALID_STATE return value */
    err = app_health_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app health task again: %s", esp_err_to_name(err));
    }
#endif
}
