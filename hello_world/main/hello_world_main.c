/* Hello World with LVGL and SSD1306.

   This exampled code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "digital_clock.h"

/* Globals */
char strftime_buf[64];  // store formatted timeinfo

static void print_chip_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
}

static void set_time_zone(char * tz_name)
{
	time_t now;
	struct tm timeinfo;

    time(&now);
    setenv("TZ", tz_name, 1); // Central European Summer Time
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf("The current system time: %s\n", strftime_buf);

    ESP_LOGI(TAG, "current system time: %s\n", strftime_buf);
}

static void sys_time_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* data)
{
	time_t now;
	struct tm timeinfo;

	time(&now);
	localtime_r(&now, &timeinfo);
	// %d: day of month (01-31)
	// %m: month as decimal number (01-12)
	// %Y: year
	// %X : time representation
	strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y\n%X", &timeinfo);

	//ESP_LOGI(TAG, "%s: called sys_time_event_handler", strftime_buf);
	dc_display_text(strftime_buf);
}

void app_main(void)
{
    /* Print chip information */
	print_chip_info();

    /* Set the system time zone */
	set_time_zone("CEST+2");

	/* Initialize a digital clock handler */
	clock_handle_t* my_clock = dc_create_clock_handle();

	/* Register an event handler for clock handler */
	dc_add_event_handler(my_clock, sys_time_event_handler, NULL);

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	ESP_LOGI(TAG, "main loop");

	while (1) {
		vTaskDelay(portMAX_DELAY);
	}

	/* Unregister the event handler */
	dc_remove_event_handler(my_clock, sys_time_event_handler);

	/* Delete the digital clock handler */
	dc_delete_clock_handle(my_clock);
}


