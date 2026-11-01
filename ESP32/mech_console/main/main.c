/**
 * @file    main.c
 * @author  maxin
 * @date    2026-11-01
 * @brief   Main application entry point.
 *
 * SPDX-License-Identifier: MIT
 */

#include <inttypes.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "app_command.h"
#include "app_health.h"
#include "app_link.h"
#include "app_protocol.h"
#include "app_rgb.h"
#include "app_session.h"
#include "app_uart.h"
#include "frame_packet.h"

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP");

static void print_chip_info(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s, ", CONFIG_IDF_TARGET, chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(TAG, "silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }

    ESP_LOGI(TAG, "%" PRIu32 "MB %s flash", flash_size / (uint32_t)(1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
}

static void on_packet(const frame_packet_t *packet, void *context)
{
    (void)context;

    esp_err_t err = app_command_submit(packet);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to submit packet to app command module: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    /* Print chip information */
    print_chip_info();

    esp_err_t err = app_health_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app health task: %s", esp_err_to_name(err));
        return;
    }

    err = app_uart_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app UART: %s", esp_err_to_name(err));
        return;
    }

    err = app_command_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app command module: %s", esp_err_to_name(err));
        return;
    }

    err = app_link_start(on_packet, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start app link: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Application link ready");

    err = app_rgb_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize app RGB module: %s", esp_err_to_name(err));
        return;
    }

    /* Use a dim green LED to indicate successful initialization. */
    err = app_rgb_set(0, 8, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RGB LED color: %s", esp_err_to_name(err));
        return;
    }
}
