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

static const char *TAG = "esp32";

static EventGroupHandle_t wifi_event_group;

static RTC_DATA_ATTR char wifi_ssid[32];
static RTC_DATA_ATTR char wifi_password[64];

#define ap_ssid "esp32"
#define ap_password "88888888"

static esp_err_t server_page_get_handler(httpd_req_t *req);
static esp_err_t wifi_info_get_handler(httpd_req_t *req);


static const httpd_uri_t server_page = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = server_page_get_handler,
	.user_ctx = NULL,
};

static const httpd_uri_t wifi_info = {
	.uri = "/connection",
	.method = HTTP_POST,
	.handler = wifi_info_get_handler,
	.user_ctx = "TEST",
};


static esp_err_t server_page_get_handler(httpd_req_t *req)
{
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");

    httpd_resp_sendstr_chunk(req, "<head>");
    httpd_resp_sendstr_chunk(req, "<style>");
    httpd_resp_sendstr_chunk(req, "form {display: grid;padding: 1em; background: #f9f9f9; border: 1px solid #c1c1c1; margin: 2rem auto 0 auto; max-width: 400px; padding: 1em;}}");
    httpd_resp_sendstr_chunk(req, "form input {background: #fff;border: 1px solid #9c9c9c;}");
    httpd_resp_sendstr_chunk(req, "form button {background: lightgrey; padding: 0.7em;width: 100%; border: 0;");
    httpd_resp_sendstr_chunk(req, "label {padding: 0.5em 0.5em 0.5em 0;}");
    httpd_resp_sendstr_chunk(req, "input {padding: 0.7em;margin-bottom: 0.5rem;}");
    httpd_resp_sendstr_chunk(req, "input:focus {outline: 10px solid gold;}");
    httpd_resp_sendstr_chunk(req, "@media (min-width: 300px) {form {grid-template-columns: 200px 1fr; grid-gap: 16px;} label { text-align: right; grid-column: 1 / 2; } input, button { grid-column: 2 / 3; }}");
    httpd_resp_sendstr_chunk(req, "</style>");
    httpd_resp_sendstr_chunk(req, "</head>");

    httpd_resp_sendstr_chunk(req, "<body>");
    httpd_resp_sendstr_chunk(req, "<form class=\"form1\" id=\"loginForm\" action=\"\">");

    httpd_resp_sendstr_chunk(req, "<label for=\"SSID\">WiFi Name</label>");
    httpd_resp_sendstr_chunk(req, "<input id=\"ssid\" type=\"text\" name=\"ssid\" maxlength=\"64\" minlength=\"4\">");

    httpd_resp_sendstr_chunk(req, "<label for=\"Password\">Password</label>");
    httpd_resp_sendstr_chunk(req, "<input id=\"pwd\" type=\"password\" name=\"pwd\" maxlength=\"64\" minlength=\"4\">");

    httpd_resp_sendstr_chunk(req, "<button>Submit</button>");
    httpd_resp_sendstr_chunk(req, "</form>");

    httpd_resp_sendstr_chunk(req, "<script>");
    httpd_resp_sendstr_chunk(req, "document.getElementById(\"loginForm\").addEventListener(\"submit\", (e) => {e.preventDefault(); const formData = new FormData(e.target); const data = Array.from(formData.entries()).reduce((memo, pair) => ({...memo, [pair[0]]: pair[1],  }), {}); var xhr = new XMLHttpRequest(); xhr.open(\"POST\", \"http://192.168.1.1/connection\", true); xhr.setRequestHeader('Content-Type', 'application/json'); xhr.send(JSON.stringify(data)); document.getElementById(\"output\").innerHTML = JSON.stringify(data);});");
    httpd_resp_sendstr_chunk(req, "</script>");

    httpd_resp_sendstr_chunk(req, "</body></html>");

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t wifi_info_get_handler(httpd_req_t *req)
{
	char buf[128];
	int ret = 0;
	int remain = req->content_len;

	while (remain > 0) {
		/* read data from the request */
		ret = httpd_req_recv(req, buf, MIN(remain, sizeof(buf)));
		if (ret <= 0)
			return ESP_FAIL;

		/* parse data */
		ESP_LOGI(TAG, "%.*s", ret, buf);

		cJSON *root = cJSON_Parse(buf);
		sprintf(wifi_ssid, "%s",
				cJSON_GetObjectItem(root, "ssid")->valuestring);
		sprintf(wifi_password, "%s",
				cJSON_GetObjectItem(root, "pwd")->valuestring);
		ESP_LOGI(TAG, "wifi ssid: %s", wifi_ssid);
		ESP_LOGI(TAG, "wifi password: %s", wifi_password);

		remain -= ret;
	}

	/* end response */
	httpd_resp_send_chunk(req, NULL, 0);
	esp_sleep_enable_timer_wakeup(100000);
	esp_deep_sleep_start();
	return ESP_OK;

}

static void wifi_event_handler(void *handler_arg, esp_event_base_t base,
							   int32_t id, void *event_data)
{
	wifi_event_ap_staconnected_t *event;

	switch (id) {
	case WIFI_EVENT_AP_STACONNECTED:
		event = (wifi_event_ap_staconnected_t *)event_data;
		ESP_LOGI(TAG, "station " MACSTR " join, AID = %d",
				 MAC2STR(event->mac), event->aid);
		ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED event handled");
		break;

	case WIFI_EVENT_AP_STADISCONNECTED:
	 	event = (wifi_event_ap_staconnected_t *)event_data;
		ESP_LOGI(TAG, "station " MACSTR " level, AID = %d",
				 MAC2STR(event->mac), event->aid);
		ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED event handled");
		break;
	}
}

static void webserver_start()
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	if (ESP_OK == httpd_start(&server, &config)) {
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &server_page));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_info));
		ESP_LOGI(TAG, "start web server.");
	}
}

static esp_err_t setup()
{
	esp_err_t ret;
	
	ret = nvs_flash_init();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
		ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
	}

	ESP_LOGI(TAG, "nvs_flash_init() done.");

	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_netif_init());

	wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&config));

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
					&wifi_event_handler, NULL));

	wifi_config_t ap_config = {
		.ap = {
			.ssid = ap_ssid,
			.ssid_len = strlen(ap_ssid),
			.password = ap_password,
			.max_connection = 1,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK,
		},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta done.");

	webserver_start();

	return ret;
}

void app_main()
{
	esp_chip_info_t chip_info;
	uint32_t flash_size;
	unsigned int marjor_rev;
	unsigned int minor_rev; 

	printf("Hello world\n");

	/* print chip information */
	esp_chip_info(&chip_info);
	printf("this is %s chip with %d cpu core(s), WiFi%s%s, ",
		   CONFIG_IDF_TARGET, chip_info.cores,
		   (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
		   (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	marjor_rev = chip_info.revision / 100;
	minor_rev = chip_info.revision % 100;
	printf("silicon revision v%d.%d, ", marjor_rev, minor_rev);
	if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
		printf("get flash size failed\n");
		return;
	}

	printf("%luMB %s flash\n", flash_size >> 20,
		   (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "emmbedded" : "external");

	printf("minimum free heap size: %ld bytes\n", esp_get_minimum_free_heap_size());

	setup();
}
