#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_sleep.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "cJSON.h"
#include "wifi_manager.h"
#include "http_app.h"
#include "mqtt.h"

#include "led_strip.h"
#include "led_strip_rmt.h"
#include "led_strip_interface.h"

static const char TAG[] = "any_gateway";
static struct EventGroupDef_t *wifi_event_group;

/* wifi event define */
const EventBits_t WIFI_CONNECTED = BIT0;


static void cb_wifi_connectted(void *parameter)
{
	ESP_LOGI(TAG, "wifi connected callback");
	xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED);
}

#define BLINK_GPIO 13

led_strip_handle_t led_strip;

/* LED strip initialization with the GPIO and pixels number*/
led_strip_config_t strip_config = {
    .strip_gpio_num = BLINK_GPIO, // The GPIO that connected to the LED strip's data line
    .max_leds = 12, // The number of LEDs in the strip,
    .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
    .led_model = LED_MODEL_WS2812, // LED strip model
    .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
};

led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    .rmt_channel = 0,
#else
    .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false, // whether to enable the DMA feature
#endif
};


void app_main()
{
	wifi_event_group = xEventGroupCreate();
	led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
	for (int i = 0; i < 12; i++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 5, 5, 5));
    }
	led_strip_refresh(led_strip);
	wifi_manager_start();
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, cb_wifi_connectted);
	xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
	mqtt_client_init();
}
