#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

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

	for (int i = 10; i >= 0; i--) {
		printf("restarting in %d seconds...\n", i);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	printf("Resarting now.\n");
	fflush(stdout);
	esp_restart();
}

