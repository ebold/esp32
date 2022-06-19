/*
 * digital_clock.c
 *
 *  Created on: Jun 12, 2022
 *      Author: ebold
 */
#include "digital_clock.h"
#include "hello_world.h"

ESP_EVENT_DEFINE_BASE(MY_DIGITAL_CLOCK_EVENT);

/* Function prototypes */
static void lv_tick_task(void *arg);
static lv_obj_t* create_screen_label(lv_disp_t* screen);
static void display_task_entry(void* arg);

void lv_tick_task(void *arg)
{
	(void) arg;
	//ESP_LOGI(TAG, "tick");
	lv_tick_inc(LV_TICK_PERIOD_MS);
}

clock_handle_t* dc_create_clock_handle(void)
{
	clock_handle_t *my_clock = calloc(1, sizeof(clock_handle_t));
	if (!my_clock) {
		ESP_LOGE(TAG, "calloc memory for digital clock failed");
	    return NULL;
	}

	my_clock->semaphore = xSemaphoreCreateMutex(); // for concurrent calls of lv_* functions

	// Initialize LVGL
	lv_init();

	// Initialize the SPI or I2C bus used by the drivers
	lvgl_driver_init();

	// Initialize the frame buffers
	lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
	if(!buf1) {
		ESP_LOGE(TAG, "malloc memory for digital clock (buf1) failed");
		return NULL;
	}
	my_clock->buf1 = buf1;

	// Use double buffers if not working with monochrome displays
	lv_color_t* buf2 = NULL;
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
	if(!buf2) {
		ESP_LOGE(TAG, "malloc memory for digital clock (buf2) failed");
		return NULL;
	}
	my_clock->buf2 = buf2;
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
	static lv_disp_drv_t disp_drv;
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

	/* Create an event loop */
	esp_event_loop_args_t loop_args = {
			.queue_size = 2,
			.task_name = NULL,
			.task_priority = 0,
			.task_stack_size = 2048
	};

	esp_event_loop_create(&loop_args, &my_clock->event_loop_hdl);

	/* Create digital clock task */
	BaseType_t err = xTaskCreate(
			display_task_entry,
			"display_task",
			4096 *2,
			my_clock,
			1,
			&my_clock->task_hdl);

	if (err != pdTRUE) {
		ESP_LOGE(TAG, "create clock handle failed");
		return NULL;
	}
	ESP_LOGI(TAG, "clock handle created");

	return my_clock;
}

void dc_delete_clock_handle(clock_handle_t* clock_hdl)
{

	free(clock_hdl->buf1);

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
	free(clock_hdl->buf2);
#endif

	vSemaphoreDelete(clock_hdl->semaphore);

	vTaskDelete(clock_hdl->task_hdl);

	esp_event_loop_delete(clock_hdl->event_loop_hdl);

	ESP_LOGI(TAG, "clock handle deleted");
}

lv_obj_t* create_screen_label(lv_disp_t* screen)
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

void dc_display_text(char *disp_text)
{
    // Modify the Label's text
    lv_label_set_text(screen_label, disp_text);
    ESP_LOGI(TAG, "display: %s\n", disp_text);
}

static void display_task_entry(void* arg)
{
	clock_handle_t *my_clock = (clock_handle_t*)arg;

	// Create and start a timer to call lv_tick_inc()
	const esp_timer_create_args_t disp_timer_args = {
			.callback = &lv_tick_task,
			.name = "disp_timer",
	};

	esp_timer_handle_t disp_timer;
	ESP_ERROR_CHECK(esp_timer_create(&disp_timer_args, &disp_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(disp_timer, LV_TICK_PERIOD_MS * 1000)); // 1 ms period

	// Create a screen label
	screen_label = create_screen_label(NULL);
	assert(screen_label != NULL);

	// Display a given text on screen
	lv_label_set_text(screen_label, "Hello, there!");

	// Task loop
	while(1) {
		// Delay 1 tick (assume FreeRTOS has 10 ms tick by default)
		vTaskDelay(pdMS_TO_TICKS(10));

		// Try to take the semaphore, on success call lv_* function
		if (xSemaphoreTake(my_clock->semaphore, portMAX_DELAY) == pdTRUE) {
			lv_task_handler();

			// Give the semaphore
			xSemaphoreGive(my_clock->semaphore);
		}

		esp_event_post_to(my_clock->event_loop_hdl, MY_DIGITAL_CLOCK_EVENT, SYSTEM_TIME_UPDATE, NULL, 0, pdMS_TO_TICKS(1000));

		esp_event_loop_run(my_clock->event_loop_hdl, pdMS_TO_TICKS(1000));
	}
}

esp_err_t dc_add_event_handler(clock_handle_t* clock_hdl, esp_event_handler_t event_handler, void *handler_args)
{
	esp_err_t err = esp_event_handler_register_with(
			clock_hdl->event_loop_hdl,
			MY_DIGITAL_CLOCK_EVENT, ESP_EVENT_ANY_ID,
			event_handler, handler_args);

	if (err != ESP_OK)
		ESP_LOGE(TAG, "add event handler failed");
	else
		ESP_LOGI(TAG, "event handler added");

	return err;
}

esp_err_t dc_remove_event_handler(clock_handle_t* clock_hdl, esp_event_handler_t event_handler)
{
	esp_err_t err = esp_event_handler_unregister_with(
			clock_hdl->event_loop_hdl,
			MY_DIGITAL_CLOCK_EVENT, ESP_EVENT_ANY_ID,
			event_handler);

	if (err != ESP_OK)
		ESP_LOGE(TAG, "removing an event handler failed");
	else
		ESP_LOGI(TAG, "event handler removed");

	return err;
}
