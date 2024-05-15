#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "cJSON.h"

#include "mqtt.h"


static const char *TAG = "mqtt";

static EventGroupHandle_t mqtt_event_group;

static esp_mqtt_client_handle_t client;

/* mqtt event define */
const EventBits_t MQTT_CONNECTED = BIT0;
const EventBits_t MQTT_DISCOVERY = BIT1;

struct hass_discovery {
	const char *discovery_prefix;
	const char *component;
	const char *node_id;
	const char *object_id;

	const char *name;
	const char *sw_version;
	const char *support_url;
	const char *manufacture;
	const char *hw_version;
	const char *suggest_area;
	const char *serial_number;
	const char *unique_id;
	const char *device_class;
	const char *state_topic;
	const char *command_topic;
};

struct mqtt_device {
	char *discovery_topic;
	char *state_topic;
	char *command_topic;
	struct hass_discovery hass_discovery;
};

static struct mqtt_device * mqtt_device_create(void)
{
	struct mqtt_device *dev;
	const char *discovery_path = "conifg";
	const char *state_path = "state";
	const char *command_path = "set";
	size_t topic_len = 0;

	dev = malloc(sizeof(struct mqtt_device));

	dev->hass_discovery.discovery_prefix = "homeassistant";
	dev->hass_discovery.component = "light";
	dev->hass_discovery.node_id = "1234";
	dev->hass_discovery.object_id = "1234";

	/* discovery topic format: <discovery_prefix>/<component>/[<node_id>/]<object_id>/config */
	/* see: https://www.home-assistant.io/integrations/mqtt/ */
	topic_len = strlen(dev->hass_discovery.discovery_prefix) +
				strlen(dev->hass_discovery.component) +
				strlen(dev->hass_discovery.node_id) +
				strlen(dev->hass_discovery.object_id) +
				strlen(discovery_path) + 4 + 1;


	dev->discovery_topic = malloc(topic_len);
	sprintf(dev->discovery_topic, "%s/%s/%s/%s/%s",
			dev->hass_discovery.discovery_prefix,
			dev->hass_discovery.component,
			dev->hass_discovery.node_id,
			dev->hass_discovery.object_id,
			discovery_path);

	ESP_LOGI(TAG, "discovery topic len: %d", topic_len);
	ESP_LOGI(TAG, "discovery topic: %s", dev->discovery_topic);

	dev->hass_discovery.name = "rgb light";
	dev->hass_discovery.device_class = "light";
	dev->hass_discovery.sw_version = "v1.0.0";
	dev->hass_discovery.hw_version = "v1.0.0";
	dev->hass_discovery.manufacture = "jack";
	dev->hass_discovery.serial_number = "12345678";
	dev->hass_discovery.suggest_area = "living room";
	dev->hass_discovery.support_url = "192.168.1.212";
	dev->hass_discovery.unique_id =  "light01";

	/* state topic format: <discovery_prefix>/<component>/[<node_id>/]<object_id>/state */
	topic_len = strlen(dev->hass_discovery.discovery_prefix) +
				strlen(dev->hass_discovery.component) +
				strlen(dev->hass_discovery.node_id) +
				strlen(dev->hass_discovery.object_id) +
				strlen(state_path) + 4 + 1;
	dev->state_topic = malloc(topic_len);
	sprintf(dev->state_topic, "%s/%s/%s/%s/%s",
			dev->hass_discovery.discovery_prefix,
			dev->hass_discovery.component,
			dev->hass_discovery.node_id,
			dev->hass_discovery.object_id,
			state_path);

	dev->hass_discovery.state_topic = dev->state_topic;

	/* state topic format: <discovery_prefix>/<component>/[<node_id>/]<object_id>/set */
	topic_len = strlen(dev->hass_discovery.discovery_prefix) +
				strlen(dev->hass_discovery.component) +
				strlen(dev->hass_discovery.node_id) +
				strlen(dev->hass_discovery.object_id) +
				strlen(command_path) + 4 + 1;
	dev->command_topic = malloc(topic_len);
	sprintf(dev->command_topic, "%s/%s/%s/%s/%s",
			dev->hass_discovery.discovery_prefix,
			dev->hass_discovery.component,
			dev->hass_discovery.node_id,
			dev->hass_discovery.object_id,
			command_path);

	dev->hass_discovery.command_topic = dev->command_topic;

	return dev;
}

/* mqtt config */
static struct esp_mqtt_client_config_t mqtt_config = {
	.broker.address.uri = "mqtt://192.168.1.234",
	.broker.address.port = 1883,
	.credentials.username = "homeassistant",
	.credentials.authentication.password = "mi3oonuyei9pahphaemooJeem0oor6yaiThui2esh5fee8vae8Ahthaic8Ohnoh4",
};

static struct mqtt_device *test_dev;

static char * discovery_json_create(struct hass_discovery *discovery)
{
	char *json = NULL;

	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "name", discovery->name);
	cJSON_AddStringToObject(root, "schema", "json");
	cJSON_AddStringToObject(root, "device_class", discovery->device_class);
	cJSON_AddStringToObject(root, "sw_version", discovery->sw_version);
	cJSON_AddStringToObject(root, "hw_version", discovery->hw_version);
	cJSON_AddStringToObject(root, "manufacture", discovery->manufacture);
	cJSON_AddStringToObject(root, "serial_number", discovery->serial_number);
	cJSON_AddStringToObject(root, "sugest_area", discovery->suggest_area);
	cJSON_AddStringToObject(root, "support_url", discovery->suggest_area);
	cJSON_AddStringToObject(root, "unique_id", discovery->unique_id);
	cJSON_AddStringToObject(root, "state_topic", discovery->state_topic);
	cJSON_AddStringToObject(root, "command_topic", discovery->command_topic);
	
	json = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

	return json;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
	if (error_code != 0) {
		ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
	}
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
							   int32_t event_id, void *event_data)
{
	ESP_LOGI(TAG,
			 "Event dispatched from event loop base=%s, event_id=%" PRIi32 "",
			 base, event_id);
	esp_mqtt_event_handle_t event = event_data;

	switch ((esp_mqtt_event_id_t)event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED);
		xEventGroupSetBits(mqtt_event_group, MQTT_DISCOVERY);
		break;

	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED);
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA");
		printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		printf("DATA=%.*s\r\n", event->data_len, event->data);
		break;

	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
			log_error_if_nonzero("reported from esp-tls",
								 event->error_handle->esp_tls_last_esp_err);
			log_error_if_nonzero("reported from tls stack",
								 event->error_handle->esp_tls_stack_err);
			log_error_if_nonzero("captured as transport's socket errno",
								 event->error_handle->esp_transport_sock_errno);
			ESP_LOGI(TAG, "Last errno string (%s)",
								 strerror(event->error_handle->esp_transport_sock_errno));
		}
		break;

	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
}

void task_discovery_publish(void *param)
{
	char *json = NULL;

	while (true) {
		xEventGroupWaitBits(mqtt_event_group, MQTT_DISCOVERY, false, true, portMAX_DELAY);
		json = discovery_json_create(&test_dev->hass_discovery);
		ESP_LOGI(TAG, "[MQTT] Publish topic %s data %.*s",
				 test_dev->discovery_topic, strlen(json), json);
		esp_mqtt_client_publish(client, test_dev->discovery_topic, json, 0, 0, true);
		xEventGroupClearBits(mqtt_event_group, MQTT_DISCOVERY);
		free(json);
	}
}

void mqtt_client_init(void)
{
	esp_log_level_set(TAG, ESP_LOG_VERBOSE);
	mqtt_event_group = xEventGroupCreate();
	
	client = esp_mqtt_client_init(&mqtt_config);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);
	ESP_LOGI(TAG, "connecting to %s", mqtt_config.broker.address.uri);

	test_dev = mqtt_device_create();
	xTaskCreate(task_discovery_publish, "discovery publish", 2048, NULL, 0, NULL);
}