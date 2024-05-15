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
#include "mqtt.h"

static const char TAG[] = "any_gateway";
static struct EventGroupDef_t *wifi_event_group;

/* wifi event define */
const EventBits_t WIFI_CONNECTED = BIT0;


static void cb_wifi_connectted(void *parameter)
{
	ESP_LOGI(TAG, "wifi connected callback");
	xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED);
}

void app_main()
{
	wifi_event_group = xEventGroupCreate();
	wifi_manager_start();
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, cb_wifi_connectted);
	xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
	mqtt_client_init();
}
