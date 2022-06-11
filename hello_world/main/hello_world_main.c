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
#include "esp_spi_flash.h"
#include "esp_event.h"
#include "time.h"

/* Define system time base event */
ESP_EVENT_DEFINE_BASE(ESP_SYSTEM_TIME_EVENT);

typedef enum {
	SYSTEM_TIME_UPDATE
} sys_time_evt_id_t;

#include "esp_log.h"
/* FreeRTOS specific */
#ifdef CONFIG_FREEROTS_UNICORE

static const BaseType_t app_cpu = 0;

#else

static const BaseType_t app_cpu = 1;

#endif

/* LVLG relevant */
#include "freertos/semphr.h"
#include "esp_freertos_hooks.h"
#include "lvgl_helpers.h"

#ifdef LV_LVGL_H_INCLUDE_SIMPLE

#include "lvgl.h"
#include "demos/lv_demos.h"

#else

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"

#endif

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    #if defined CONFIG_LV_USE_DEMO_WIDGETS
        #include "lv_examples/src/lv_demo_widgets/lv_demo_widgets.h"
    #elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
        #include "lv_examples/src/lv_demo_keypad_encoder/lv_demo_keypad_encoder.h"
    #elif defined CONFIG_LV_USE_DEMO_BENCHMARK
        #include "lv_examples/src/lv_demo_benchmark/lv_demo_benchmark.h"
    #elif defined CONFIG_LV_USE_DEMO_STRESS
        #include "lv_examples/src/lv_demo_stress/lv_demo_stress.h"
    #else
        #error "No demo application selected."
    #endif
#endif

/* Defines */
#define TAG "lvgl_demo"
#define LV_TICK_PERIOD_MS 1

/* Custom defines for SSD1306 resolution
 *
 * LV_HOR_RES_MAX and LV_VER_RES_MAX are not set in sdkconfig since LVGL v8.0
 */
#define SSD1306_HOR_RES_MAX 128
#define SSD1306_VER_RES_MAX 64

lv_obj_t *screen_label;

/* Function prototypes */
static void lv_tick_task(void *arg);
static void display_task(void *pvParameter);
static lv_obj_t* create_screen_label(lv_disp_t* screen);
static void display_text(lv_obj_t* label, char *disp_text);
static void sys_time_evt_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* data);

/* A semaphore is used to handle concurrent calls to
 * lv_* functions, which are not thread-safe.
 */

SemaphoreHandle_t xGuiSemaphore;

/*
 * System time
 */

time_t now;
char strftime_buf[64];  // store formatted timeinfo
struct tm timeinfo;

void app_main(void)
{
    /* Print chip information */
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

    /* Get the system time and output to log */
    time(&now);
    setenv("TZ", "CEST+2", 1); // Central European Summer Time
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf("The current system time: %s\n", strftime_buf);

    ESP_LOGI(TAG, "current system time: %s\n", strftime_buf);

    xTaskCreatePinnedToCore(display_task,
		"display_task",
		4096 *2,
		NULL,
		0,
		NULL,
		app_cpu);
}

static void lv_tick_task(void *arg)
{
	(void) arg;
	lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void display_task(void *pvParameter)
{
	(void) pvParameter;
	xGuiSemaphore = xSemaphoreCreateMutex();

	// Initialize LVGL
	lv_init();

	// Initialize the SPI or I2C bus used by the drivers
	lvgl_driver_init();

	// Initialize the frame buffers
	lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
	assert(buf1 != NULL);

	// Use double buffers if not working with monochrome displays
	lv_color_t* buf2 = NULL;
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
	assert(buf2 != NULL);
#endif

	static lv_disp_draw_buf_t disp_buf;
	uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820 \
	    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A \
	    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D \
	    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
	// actual size in pixels, not bytes
	size_in_px *= 8;
#endif

	// Initialize the working buffer depending on the selected display.
	// Note: buf2 == NULL, if monochrome display is used
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

	// Initialize the display driver
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.flush_cb = disp_driver_flush;  // callback to flush frame buffers

	// When using a monochrome display, register the callbacks
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	disp_drv.rounder_cb = disp_driver_rounder;
	disp_drv.set_px_cb = disp_driver_set_px;
#endif

	disp_drv.draw_buf = &disp_buf;
	disp_drv.hor_res = SSD1306_HOR_RES_MAX;
	disp_drv.ver_res = SSD1306_VER_RES_MAX;
	lv_disp_drv_register(&disp_drv);

	// Create and start a timer to call lv_tick_inc()
	const esp_timer_create_args_t disp_timer_args = {
			.callback = &lv_tick_task,
			.name = "disp_timer",
	};

	esp_timer_handle_t disp_timer;
	ESP_ERROR_CHECK(esp_timer_create(&disp_timer_args, &disp_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(disp_timer, LV_TICK_PERIOD_MS * 1000));

	// Create a screen label
	screen_label = create_screen_label(NULL);
	assert(screen_label != NULL);

	// Display a given text on screen
	display_text(screen_label, "Good morning!");

	/* Create an event loop */
	esp_event_loop_args_t loop_args = {
			.queue_size = 2,
			.task_name = NULL,
			.task_priority = 0,
			.task_stack_size = 2048,
			.task_core_id = app_cpu
	};

	esp_event_loop_handle_t loop_handle;
	esp_event_loop_create(&loop_args, &loop_handle);

	esp_event_handler_register_with(loop_handle,
			ESP_SYSTEM_TIME_EVENT, ESP_EVENT_ANY_ID,
			sys_time_evt_handler, NULL);

	// Task loop
	while(1) {
		// Delay 1 tick (assume FreeRTOS has 10 ms tick by default)
		vTaskDelay(pdMS_TO_TICKS(10));

		// Try to take the semaphore, on success call lv_* function
		if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
			lv_task_handler();

			// Give the semaphore
			xSemaphoreGive(xGuiSemaphore);
		}

		esp_event_post_to(loop_handle, ESP_SYSTEM_TIME_EVENT, SYSTEM_TIME_UPDATE, NULL, 0, pdMS_TO_TICKS(1000));

		esp_event_loop_run(loop_handle, pdMS_TO_TICKS(1000));

	}

	// This task should NEVER run here
	free(buf1);

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	free(buf2);
#endif
	vTaskDelete(NULL);
}

static lv_obj_t* create_screen_label(lv_disp_t* screen)
{
	lv_obj_t* label;

	// Monochrome display
#if defined CONFIG_LV_TFT_DISPLAY_MONOCHROME || \
    defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S

    // Get the current screen
    lv_obj_t * scr = lv_disp_get_scr_act(screen);

    // Create a Label on the currently active screen
    label =  lv_label_create(scr);

    // Modify the Label's text
    //lv_label_set_long_mode(label1, LV_LABEL_LONG_SCROLL_CIRCULAR); // circular scrolling
	//lv_obj_set_width(label1, 96);
    //lv_label_set_text(label1, disp_text);

    // Align the Label to the center
    // NULL means align on parent (which is the screen now)
    // 0, 0 at the end means an x, y offset after alignment
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

#endif
    // debug output
    ESP_LOGI(TAG, "created a label");

    return label;
}

static void display_text(lv_obj_t* label, char *disp_text)
{
    // Modify the Label's text
    lv_label_set_text(label, disp_text);
    ESP_LOGI(TAG, "display: %s\n", disp_text);
}

static void sys_time_evt_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* data)
{
	time(&now);
	localtime_r(&now, &timeinfo);
	// %d: day of month (01-31)
	// %m: month as decimal number (01-12)
	// %Y: year
	// %X : time representation
	strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y\n%X", &timeinfo);

	ESP_LOGI(TAG, "time_evt_handler");
	display_text(screen_label, strftime_buf);
}
