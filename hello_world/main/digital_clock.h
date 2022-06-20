/*
 * digital_clock.h
 *
 * Derived from peripheral example [1].
 *
 *  Created on: Jun 12, 2022
 *      Author: ebold
 *
 * [1] NMEA parser example, https://github.com/espressif/esp-idf/tree/v4.4.1/examples/peripherals/uart/nmea0183_parser
 */

#ifndef MAIN_DIGITAL_CLOCK_H_
#define MAIN_DIGITAL_CLOCK_H_

#include "stdio.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_event.h"
#include "time.h"

/* Define system time base event */
ESP_EVENT_DECLARE_BASE(MY_DIGITAL_CLOCK_EVENT);

typedef enum {
	SYSTEM_TIME_UPDATE
} sys_time_evt_id_t;

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

#define LV_TICK_PERIOD_MS 1  // 1 ms tick

/* Custom defines for SSD1306 resolution
 *
 * LV_HOR_RES_MAX and LV_VER_RES_MAX are not set in sdkconfig since LVGL v8.0
 */
#define SSD1306_HOR_RES_MAX 128
#define SSD1306_VER_RES_MAX 64

lv_obj_t *screen_label;
extern char strftime_buf[64];  // store formatted timeinfo

/* Digital clock run-time structure */
typedef struct {
    esp_event_loop_handle_t event_loop_hdl;    // system time update event handler
    TaskHandle_t task_hdl;                     // display task handler
    QueueHandle_t event_queue;                 // event queue
    SemaphoreHandle_t semaphore;               // vl_* semaphore
    lv_color_t* buf1;                          // frame buffer
    lv_color_t* buf2;                          // frame buffer
    lv_obj_t *label;                           // screen label
    BaseType_t app_cpu;                        // application CPU
} clock_handle_t;

/* Function prototypes */
void dc_display_text(char *disp_text);
clock_handle_t* dc_create_clock_handle(void);
void dc_delete_clock_handle(clock_handle_t* clock_hdl);
esp_err_t dc_add_event_handler(clock_handle_t* clock_hdl, esp_event_handler_t event_handler, void *handler_args);
esp_err_t dc_remove_event_handler(clock_handle_t* clock_hdl, esp_event_handler_t event_handler);

/* A semaphore is used to handle concurrent calls to
 * lv_* functions, which are not thread-safe.
 */

SemaphoreHandle_t xGuiSemaphore;

#endif /* MAIN_DIGITAL_CLOCK_H_ */
