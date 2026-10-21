#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"

#include "app_link.h"
#include "app_uart.h"
#include "app_uart_config.h"
#include "frame_stream.h"

static const char *TAG = "UART_LINK_TEST";

/* Test-only responder: production app_link remains receive-only. */
static void on_packet(const frame_packet_t *packet, void *context)
{
    (void)context;
    uint8_t wire[FRAME_STREAM_MAX_ENCODED_SIZE + 1];
    size_t length = 0;
    if (frame_packet_encode(packet, wire, sizeof(wire), &length) != FRAME_CODEC_OK) {
        ESP_LOGE(TAG, "Could not encode reply");
        abort();
    }
    size_t offset = 0;
    while (offset < length) {
        int written = uart_write_bytes(APP_UART_NUM, wire + offset, length - offset);
        if (written <= 0) {
            ESP_LOGE(TAG, "Could not send reply");
            abort();
        }
        offset += (size_t)written;
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_uart_start());
    ESP_ERROR_CHECK(uart_flush_input(APP_UART_NUM));
    ESP_ERROR_CHECK(app_link_start(on_packet, NULL));
    ESP_LOGI(TAG, "UART_LINK_READY");
}
