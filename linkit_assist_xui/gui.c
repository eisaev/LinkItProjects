/*
 This sample code is in public domain.

 This sample code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* 
 This sample shows a line that keeps spinning on top of a gradient
 background image.

 It decodes a PNG image resource into a frame buffer with
 vm_graphic_draw_image_memory,
 and draws a line with vm_graphic_draw_line, then start a timer with
 vm_timer_create_precise for animation.
 The timer runs every 1/10 secs, resulting in a 10 FPS animation. For each
 frame in the animation, vm_graphic_linear_transform is used to rotate
 the line, and then vm_graphic_blt_frame is called to composite the rotated line
 with the background image, and displays the result.

 Modify the macro SCREEN_WIDTH and SCREEN_HEIGHT to fit your display device.
 */

#include "vmtype.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmgraphic.h"
#include "ResID.h"
#include "vmtimer.h"
#include <math.h>
#include "lcd_sitronix_st7789s.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmdcl_pwm.h"
#include "xui.h"

/* Sample parameters */
#define SCREEN_WIDTH 240		/* Set to your display size */
#define SCREEN_HEIGHT 240		/* Set to your display size */
#define LINE_LENGTH 100			/* Length of the spinning line */
#define PI 3.14
#define FREQUENCY 180

#define BACKLIGHT_PIN VM_PIN_P1

/* Drawing resources */
vm_graphic_frame_t g_frame[2]; /* [0] for background image, [1] for the rotated line image */
vm_graphic_frame_t* g_frame_group[2]; /* For frame blt */

/* Animation timer */
VM_TIMER_ID_PRECISE g_timer_id;

/* Update the rotating line, then update the display */
static void timer_callback(VM_TIMER_ID_PRECISE tid, void* user_data) {
	vm_graphic_point_t positions[2] = { 0, };
	vm_graphic_color_argb_t color;

	/* clear the background with blue color key */
	color.a = 255;
	color.r = 0;
	color.g = 0;
	color.b = 255;
	vm_graphic_set_color(color);
	vm_graphic_draw_solid_rectangle(g_frame_group[1], 0, 0, SCREEN_WIDTH,
	SCREEN_HEIGHT);

	color.g = 255;
	vm_graphic_set_color(color);
	vm_graphic_draw_line(g_frame_group[1], SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
	SCREEN_WIDTH / 2 + LINE_LENGTH - 1, SCREEN_HEIGHT / 2);

	/* composite the rotated line image to background buffer and display it */
	vm_graphic_blt_frame(g_frame_group, positions, 2);
}

/* Prepares the first frame of animation */
static void draw_first_frame(void) {
	VMUINT8* img_data;
	VMUINT32 img_size;
	vm_graphic_color_argb_t color;

	/* draw background image */
	img_data = vm_res_get_image(IMG_ID_BG, &img_size);
	vm_graphic_draw_image_memory(g_frame_group[0], 0, 0, img_data, img_size, 0);

	/* draw a purple horizontal blue line from center of screen */
	color.a = 255;
	color.r = 0;
	color.g = 0;
	color.b = 255;
	vm_graphic_set_color(color);
	vm_graphic_draw_solid_rectangle(g_frame_group[1], 0, 0, SCREEN_WIDTH,
	SCREEN_HEIGHT);
	color.r = 255;
	vm_graphic_set_color(color);
	vm_graphic_draw_line(g_frame_group[1], SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
	SCREEN_WIDTH / 2 + LINE_LENGTH - 1, SCREEN_HEIGHT / 2);

	/* create animation timer */
	g_timer_id = vm_timer_create_precise(100, timer_callback, NULL);

	/* explicitly trigger 1st frame */
	timer_callback(g_timer_id, NULL);
}

/* Allocate memory for a single frame */
VMBOOL allocate_frame(vm_graphic_frame_t *frame) {
	if (frame == NULL) {
		return VM_FALSE;
	}

	/* We use 16-bit color, 2 bytes per pixel */
	frame->color_format = VM_GRAPHIC_COLOR_FORMAT_16_BIT;
	frame->width = SCREEN_WIDTH;
	frame->height = SCREEN_HEIGHT;
	frame->buffer_length = SCREEN_WIDTH * SCREEN_HEIGHT * 2;
	frame->buffer = vm_malloc_dma(frame->buffer_length);
	if (frame->buffer == NULL) {
		return VM_FALSE;
	}

	return VM_TRUE;
}

/* Allocate memory for all the frames we need */
VMBOOL allocate_drawing_resource(void) {
	do {
		/* Allocate buffers for each frame */
		if (!allocate_frame(&g_frame[0])) {
			break;
		}

		if (!allocate_frame(&g_frame[1])) {
			break;
		}

		/* Setup frame group for composite and display */
		g_frame_group[0] = &g_frame[0];
		g_frame_group[1] = &g_frame[1];

		return VM_TRUE;
	} while (0);

	/* Failed to allocate all buffers, free */
	return VM_FALSE;
}

/* Free one frame */
void free_frame(vm_graphic_frame_t *frame) {
	if (NULL != frame->buffer) {
		vm_free(frame->buffer);
		frame->buffer = NULL;
	}
	frame->buffer_length = 0;
}

/* Release all memory allocated for graphics frame */
void free_drawing_resource(void) {
	free_frame(&g_frame[0]);
	free_frame(&g_frame[1]);
	g_frame_group[0] = NULL;
	g_frame_group[1] = NULL;
	vm_timer_delete_precise(g_timer_id);
}

/* The callback to be invoked by the system engine. */
void handle_system_event(VMINT message, VMINT param) {
	switch (message) {
	case VM_EVENT_CREATE:
		/* Init resource for background image */
		vm_res_init(0);
		allocate_drawing_resource();
		break;
	case VM_EVENT_PAINT:
		/* Graphics library is ready to use, start drawing */
		draw_first_frame();
		break;
	case VM_EVENT_QUIT:
		free_drawing_resource();
		/* Deinit APP resource */
		vm_res_release();
		break;
	}
}

void lcd_backlight_level(VMUINT32 ulValue) {

	VM_DCL_HANDLE pwm_handle;
	vm_dcl_pwm_set_clock_t pwm_clock;
	vm_dcl_pwm_set_counter_threshold_t pwm_config_adv;

	vm_dcl_config_pin_mode(BACKLIGHT_PIN, VM_DCL_PIN_MODE_PWM);

	pwm_handle = vm_dcl_open(PIN2PWM(BACKLIGHT_PIN), vm_dcl_get_owner_id());
	vm_dcl_control(pwm_handle, VM_PWM_CMD_START, 0);
	pwm_config_adv.counter = 100;
	pwm_config_adv.threshold = ulValue;
	pwm_clock.source_clock = 0;
	pwm_clock.source_clock_division = 3;
	vm_dcl_control(pwm_handle, VM_PWM_CMD_SET_CLOCK, (void *) (&pwm_clock));
	vm_dcl_control(pwm_handle, VM_PWM_CMD_SET_COUNTER_AND_THRESHOLD,
			(void *) (&pwm_config_adv));
	vm_dcl_close(pwm_handle);
}

/* Entry point */
void vm_main(void) {
	lcd_st7789s_init();
	lcd_backlight_level(60);
	/* register system events handler */
	vm_pmng_register_system_event_callback(handle_system_event);

	// eggfly
	xui_text_view text_view;
	xui_init_text_view(&text_view);
}
