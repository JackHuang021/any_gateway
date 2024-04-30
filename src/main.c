#include <stdio.h>
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

void app_main()
{
	wifi_manager_start();
}
