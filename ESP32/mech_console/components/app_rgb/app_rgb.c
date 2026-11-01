/**
 * @file    app_rgb.c
 * @author  maxin
 * @date    2026-11-01
 * @brief   Board RGB LED initialization and color control.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"

#include "led_strip.h"

#include "app_board_config.h"
#include "app_rgb.h"

/* Application tag for logging */
ESP_LOG_ATTR_TAG(TAG, "APP_RGB");

/*! @brief Indicates whether the RGB module has been initialized */
static bool is_app_rgb_initialized = false;

/*! @brief Handle for the LED strip instance */
static led_strip_handle_t led_handle = NULL;

/** @brief Create the RMT-backed LED strip and preserve the driver error code. */
static esp_err_t configure_led(led_strip_handle_t *out_handle);

esp_err_t app_rgb_init(void)
{
    if (is_app_rgb_initialized || led_handle != NULL) {
        ESP_LOGE(TAG, "RGB module already initialized or cleanup incomplete");
        return ESP_ERR_INVALID_STATE;
    }

    /* Configure the LED strip and obtain its handle */
    esp_err_t ret = configure_led(&led_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Turn off all LEDs initially */
    ret = led_strip_clear(led_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn off LEDs: %s", esp_err_to_name(ret));
        /* Preserve the clear error even when cleanup succeeds. */
        esp_err_t cleanup_ret = led_strip_del(led_handle);
        if (cleanup_ret == ESP_OK) {
            led_handle = NULL;
        } else {
            /* Retain the handle and block reinitialization until reboot. The
             * driver may have released only part of its resources.
             */
            ESP_LOGE(TAG, "Failed to delete LED strip instance: %s", esp_err_to_name(cleanup_ret));
        }
        return ret;
    }

    is_app_rgb_initialized = true;
    return ESP_OK;
}

esp_err_t app_rgb_set(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!is_app_rgb_initialized) {
        ESP_LOGE(TAG, "RGB module not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = led_strip_set_pixel(led_handle, 0, red, green, blue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED color: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = led_strip_refresh(led_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static esp_err_t configure_led(led_strip_handle_t *out_handle)
{
    /* LED strip general initialization */
    led_strip_config_t strip_config = {
        .strip_gpio_num = APP_BOARD_RGB_PIN, /**< The GPIO that connected to the LED strip's data line */
        .max_leds = APP_BOARD_RGB_LED_COUNT, /**< The number of LEDs in the strip, */
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, /**< Pixel format of the LED strip */
        .led_model = LED_MODEL_WS2812,                               /**< LED strip model */
        .flags.invert_out = false,                                   /**< whether to invert the output signal */
    };

    /* LED strip backend configuration: RMT */
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, /**< different clock source can lead to different power consumption */
        .resolution_hz = APP_BOARD_RGB_RMT_RESOLUTION_HZ, /**< RMT counter clock frequency */
        .flags.with_dma = false,                          /**< DMA feature is available on ESP target like ESP32-S3 */
    };

    *out_handle = NULL;
    return led_strip_new_rmt_device(&strip_config, &rmt_config, out_handle);
}
