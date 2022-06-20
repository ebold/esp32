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
#include "nvs_flash.h"
#include "hello_world.h"
#include "digital_clock.h"
#include "wifi_station.h"
#include "ntp_client.h"

// [1] Time zones, https://github.com/G6EJD/ESP32-Time-Services-and-SETENV-variable

/* Globals */
const char* TZ_Europe_Berlin="CET-1CEST,M3.5.0,M10.5.0/3"; // TZ for Europe/Berlin, POSIX.1 format 2 [1]
char strftime_buf[64];       // store formatted timeinfo
static int s_retry_num = 0;  // wifi retry count

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

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

static void set_time_zone(const char * tz_name)
{
	time_t now;
	struct tm timeinfo;

    time(&now);
    setenv("TZ", tz_name, 1);
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

static void time_sync_notification_cb(struct timeval *tv)
{
	/* Notify time synchronization.
	 * The sync interval is set in menuconfig: Component config -> LWIP -> SNTP -> CONFIG_LWIP_SNTP_UPDATE_DELAY
	 * The default value is 1 hour.
	 * Use sntp_set_sync_interval(uint32_t interval_ms) and sntp_restart() to change the update interval.
	 */
	ESP_LOGI(TAG, "Notification of a SNPT time synchronization event");
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the WiFi AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void app_main(void)
{
    /* Print chip information */
	print_chip_info();

    /* Set the system time zone */
	set_time_zone(TZ_Europe_Berlin); // Central European Summer Time

	/* Initialize a digital clock handler */
	clock_handle_t* my_clock = dc_create_clock_handle();

	/* Register an event handler for clock handler */
	dc_add_event_handler(my_clock, sys_time_event_handler, NULL);

	/* Initialize NVS for WiFi credentials */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* Create the default event loop */
	// The default event loop is used for the system events (ie., WiFi, SNTP events)
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	/* Initialize a NTP client */
	nc_init(time_sync_notification_cb);

	/* Initialize a WiFi station handler */
	s_wifi_event_group = xEventGroupCreate();
	init_wifi_station(s_wifi_event_group, wifi_event_handler);

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


