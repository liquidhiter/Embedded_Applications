#include "esp_check.h"
#include "esp_log.h"

#include "app_link.h"
#include "app_uart.h"
#include "app_uart_config.h"

static const char *TAG = "UART_LINK_TEST";

/* Test-only responder using the production packet send API. */
static void on_packet(const frame_packet_t *packet, void *context)
{
    (void)context;
    ESP_ERROR_CHECK(app_link_send(packet));
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_uart_start());
    ESP_ERROR_CHECK(uart_flush_input(APP_UART_NUM));
    ESP_ERROR_CHECK(app_link_start(on_packet, NULL));
    ESP_LOGI(TAG, "UART_LINK_READY");
}
