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

/* Function prototypes */
static void lv_tick_task(void *arg);
static void gui_task(void *pvParameter);
static void create_demo(void);

/* A semaphore is used to handle concurrent calls to
 * lv_* functions, which are not thread-safe.
 */

SemaphoreHandle_t xGuiSemaphore;

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

    xTaskCreatePinnedToCore(gui_task,
		"gui",
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

static void gui_task(void *pvParameter)
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
			.name = "periodic_gui",
	};

	esp_timer_handle_t disp_timer;
	ESP_ERROR_CHECK(esp_timer_create(&disp_timer_args, &disp_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(disp_timer, LV_TICK_PERIOD_MS * 1000));

	// Create a demo application
	create_demo();

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
	}

	// This task should NEVER run here
	free(buf1);

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	free(buf2);
#endif
	vTaskDelete(NULL);
}

static void create_demo(void)
{
	// When using monochrome display, display "Hello, world!" on the center of the display.
#if defined CONFIG_LV_TFT_DISPLAY_MONOCHROME || \
    defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S

    // Get the current screen
    lv_obj_t * scr = lv_disp_get_scr_act(NULL);

    // Create a Label on the currently active screen
    lv_obj_t * label1 =  lv_label_create(scr);

    // Modify the Label's text
    lv_label_set_text(label1, "Hello\nworld!");

    // Align the Label to the center
    // NULL means align on parent (which is the screen now)
    // 0, 0 at the end means an x, y offset after alignment
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);
#else
    // Otherwise we show the selected demo

    #if defined CONFIG_LV_USE_DEMO_WIDGETS
        lv_demo_widgets();
    #elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
        lv_demo_keypad_encoder();
    #elif defined CONFIG_LV_USE_DEMO_BENCHMARK
        lv_demo_benchmark();
    #elif defined CONFIG_LV_USE_DEMO_STRESS
        lv_demo_stress();
    #else
        #error "No demo application selected."
    #endif
#endif
    // debug output
    ESP_LOGI(TAG, "created demo app");
}
