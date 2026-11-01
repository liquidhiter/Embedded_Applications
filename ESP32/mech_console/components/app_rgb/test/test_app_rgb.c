#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "unity.h"

#include "app_rgb.h"

static bool fake_driver;
static uint32_t handle_token;
static esp_err_t create_result, clear_result, delete_result, pixel_result, refresh_result;
static unsigned creates, clears, deletes, pixels, refreshes;
static uint32_t last_red, last_green, last_blue;
static led_strip_config_t captured_strip;
static led_strip_rmt_config_t captured_rmt;

esp_err_t __real_led_strip_new_rmt_device(const led_strip_config_t *, const led_strip_rmt_config_t *,
                                          led_strip_handle_t *);
esp_err_t __real_led_strip_clear(led_strip_handle_t);
esp_err_t __real_led_strip_del(led_strip_handle_t);
esp_err_t __real_led_strip_set_pixel(led_strip_handle_t, uint32_t, uint32_t, uint32_t, uint32_t);
esp_err_t __real_led_strip_refresh(led_strip_handle_t);

static led_strip_handle_t fake_handle(void)
{
    return (led_strip_handle_t)&handle_token;
}

esp_err_t __wrap_led_strip_new_rmt_device(const led_strip_config_t *strip, const led_strip_rmt_config_t *rmt,
                                          led_strip_handle_t *out)
{
    if (!fake_driver) {
        return __real_led_strip_new_rmt_device(strip, rmt, out);
    }
    ++creates;
    captured_strip = *strip;
    captured_rmt = *rmt;
    TEST_ASSERT_NULL(*out);
    if (create_result == ESP_OK) {
        *out = fake_handle();
    }
    return create_result;
}

esp_err_t __wrap_led_strip_clear(led_strip_handle_t handle)
{
    if (!fake_driver) {
        return __real_led_strip_clear(handle);
    }
    TEST_ASSERT_EQUAL_PTR(fake_handle(), handle);
    ++clears;
    return clear_result;
}

esp_err_t __wrap_led_strip_del(led_strip_handle_t handle)
{
    if (!fake_driver) {
        return __real_led_strip_del(handle);
    }
    TEST_ASSERT_EQUAL_PTR(fake_handle(), handle);
    ++deletes;
    return delete_result;
}

esp_err_t __wrap_led_strip_set_pixel(led_strip_handle_t handle, uint32_t index, uint32_t red, uint32_t green,
                                     uint32_t blue)
{
    if (!fake_driver) {
        return __real_led_strip_set_pixel(handle, index, red, green, blue);
    }
    TEST_ASSERT_EQUAL_PTR(fake_handle(), handle);
    TEST_ASSERT_EQUAL_UINT32(0, index);
    ++pixels;
    last_red = red;
    last_green = green;
    last_blue = blue;
    return pixel_result;
}

esp_err_t __wrap_led_strip_refresh(led_strip_handle_t handle)
{
    if (!fake_driver) {
        return __real_led_strip_refresh(handle);
    }
    TEST_ASSERT_EQUAL_PTR(fake_handle(), handle);
    /* Each successful pixel update must precede its refresh. */
    TEST_ASSERT_TRUE(pixels > refreshes);
    ++refreshes;
    return refresh_result;
}

static void prepare(void)
{
    /* pytest reboots before every Unity case; module state is not reset here. */
    fake_driver = true;
    create_result = clear_result = delete_result = pixel_result = refresh_result = ESP_OK;
    creates = clears = deletes = pixels = refreshes = 0;
}

TEST_CASE("RGB preserves creation errors and configures the board LED", "[app_rgb]")
{
    prepare();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_rgb_set(0, 0, 0));
    const esp_err_t errors[] = {ESP_ERR_NO_MEM, ESP_ERR_NOT_FOUND, ESP_ERR_NOT_SUPPORTED};
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        create_result = errors[i];
        TEST_ASSERT_EQUAL(errors[i], app_rgb_init());
    }
    TEST_ASSERT_EQUAL_UINT(0, clears);
    TEST_ASSERT_EQUAL_UINT(0, deletes);
    create_result = ESP_OK;
    TEST_ASSERT_EQUAL(ESP_OK, app_rgb_init());
    TEST_ASSERT_EQUAL_UINT(4, creates);
    TEST_ASSERT_EQUAL_UINT(1, clears);
    TEST_ASSERT_EQUAL_INT(8, captured_strip.strip_gpio_num);
    TEST_ASSERT_EQUAL_UINT32(1, captured_strip.max_leds);
    TEST_ASSERT_EQUAL(LED_MODEL_WS2812, captured_strip.led_model);
    TEST_ASSERT_EQUAL_UINT(1, captured_strip.color_component_format.format.r_pos);
    TEST_ASSERT_EQUAL_UINT(0, captured_strip.color_component_format.format.g_pos);
    TEST_ASSERT_EQUAL_UINT(2, captured_strip.color_component_format.format.b_pos);
    TEST_ASSERT_EQUAL_UINT(3, captured_strip.color_component_format.format.num_components);
    TEST_ASSERT_EQUAL_UINT(1, captured_strip.color_component_format.format.bytes_per_color);
    TEST_ASSERT_FALSE(captured_strip.flags.invert_out);
    TEST_ASSERT_EQUAL_UINT32(10000000, captured_rmt.resolution_hz);
    TEST_ASSERT_FALSE(captured_rmt.flags.with_dma);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_rgb_init());
    TEST_ASSERT_EQUAL_UINT(4, creates);
    fake_driver = false;
}

TEST_CASE("RGB preserves clear errors and retries after successful cleanup", "[app_rgb]")
{
    prepare();
    clear_result = ESP_ERR_TIMEOUT;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, app_rgb_init());
    TEST_ASSERT_EQUAL_UINT(1, deletes);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_rgb_set(1, 2, 3));
    clear_result = ESP_OK;
    TEST_ASSERT_EQUAL(ESP_OK, app_rgb_init());
    TEST_ASSERT_EQUAL_UINT(2, creates);
    TEST_ASSERT_EQUAL_UINT(2, clears);
    fake_driver = false;
}

TEST_CASE("RGB blocks reinitialization after cleanup failure", "[app_rgb]")
{
    prepare();
    clear_result = ESP_ERR_TIMEOUT;
    delete_result = ESP_FAIL;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, app_rgb_init());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_rgb_init());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, app_rgb_set(1, 2, 3));
    TEST_ASSERT_EQUAL_UINT(1, creates);
    TEST_ASSERT_EQUAL_UINT(1, deletes);
    TEST_ASSERT_EQUAL_UINT(0, pixels);
    fake_driver = false;
}

TEST_CASE("RGB sends channel values and propagates update failures", "[app_rgb]")
{
    prepare();
    TEST_ASSERT_EQUAL(ESP_OK, app_rgb_init());
    const uint8_t colors[][3] = {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {17, 128, 255}};
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        TEST_ASSERT_EQUAL(ESP_OK, app_rgb_set(colors[i][0], colors[i][1], colors[i][2]));
        TEST_ASSERT_EQUAL_UINT32(colors[i][0], last_red);
        TEST_ASSERT_EQUAL_UINT32(colors[i][1], last_green);
        TEST_ASSERT_EQUAL_UINT32(colors[i][2], last_blue);
    }
    unsigned before = refreshes;
    pixel_result = ESP_ERR_INVALID_ARG;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_rgb_set(1, 2, 3));
    TEST_ASSERT_EQUAL_UINT(before, refreshes);
    pixel_result = ESP_OK;
    refresh_result = ESP_ERR_TIMEOUT;
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, app_rgb_set(1, 2, 3));
    TEST_ASSERT_EQUAL_UINT(before + 1, refreshes);
    refresh_result = ESP_OK;
    TEST_ASSERT_EQUAL(ESP_OK, app_rgb_set(0, 8, 0));
    fake_driver = false;
}

TEST_CASE("RGB hardware transmits primary colors and turns off", "[app_rgb_hardware]")
{
    /* This case must only be selected on a physical board. No driver wrappers. */
    TEST_ASSERT_FALSE(fake_driver);
    TEST_ASSERT_EQUAL(ESP_OK, app_rgb_init());
    const uint8_t colors[][3] = {{8, 0, 0}, {0, 8, 0}, {0, 0, 8}};
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        TEST_ASSERT_EQUAL(ESP_OK, app_rgb_set(colors[i][0], colors[i][1], colors[i][2]));
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    TEST_ASSERT_EQUAL(ESP_OK, app_rgb_set(0, 0, 0));
    /* Driver success does not prove the emitted color; visual confirmation is separate. */
}
