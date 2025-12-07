/* Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
 * Copyright 2020 Ploopy Corporation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ploopyco.h"
#include "math.h"
#include "analog.h"
#include "opt_encoder.h"

// for legacy support
#if defined(OPT_DEBOUNCE) && !defined(PLOOPY_SCROLL_DEBOUNCE)
#    define PLOOPY_SCROLL_DEBOUNCE OPT_DEBOUNCE
#endif
#if defined(SCROLL_BUTT_DEBOUNCE) && !defined(PLOOPY_SCROLL_BUTTON_DEBOUNCE)
#    define PLOOPY_SCROLL_BUTTON_DEBOUNCE SCROLL_BUTT_DEBOUNCE
#endif

#ifndef PLOOPY_SCROLL_DEBOUNCE
#    define PLOOPY_SCROLL_DEBOUNCE 5
#endif
#ifndef PLOOPY_SCROLL_BUTTON_DEBOUNCE
#    define PLOOPY_SCROLL_BUTTON_DEBOUNCE 100
#endif

#ifndef PLOOPY_DPI_OPTIONS
#    define PLOOPY_DPI_OPTIONS \
        { 600, 900, 1200, 1600, 2400 }
#    ifndef PLOOPY_DPI_DEFAULT
#        define PLOOPY_DPI_DEFAULT 1
#    endif
#endif
#ifndef PLOOPY_SNIPE_DPI
#   define PLOOPY_SNIPE_DPI 100
#endif
#ifndef PLOOPY_DPI_DEFAULT
#    define PLOOPY_DPI_DEFAULT 0
#endif
#ifndef PLOOPY_DRAGSCROLL_DIVISOR_H
#    define PLOOPY_DRAGSCROLL_DIVISOR_H 8.0
#endif
#ifndef PLOOPY_DRAGSCROLL_DIVISOR_V
#    define PLOOPY_DRAGSCROLL_DIVISOR_V 8.0
#endif
#ifndef ENCODER_BUTTON_ROW
#    define ENCODER_BUTTON_ROW 0
#endif
#ifndef ENCODER_BUTTON_COL
#    define ENCODER_BUTTON_COL 0
#endif
#ifndef PLOOPY_SNAP_EWMA_T
#   define PLOOPY_SNAP_EWMA_T 32
#endif
#ifndef PLOOPY_SNAP_EWMA_SMP_PERIOD
#   define PLOOPY_SNAP_EWMA_SMP_PERIOD 4
#endif
#ifndef PLOOPY_SNAP_RATIO
#   define PLOOPY_SNAP_RATIO 0.5
#endif
#ifndef PLOOPY_SCROLL_DIV_OPTIONS
#   define PLOOPY_SCROLL_DIV_OPTIONS \
       { 4, 2, 1.5, 1, 0.5 }
#endif
#ifndef PLOOPY_SCROLL_DIV_DEFAULT
#   define PLOOPY_SCROLL_DIV_DEFAULT 0
#endif

#define abs(x) ((x) > 0 ? (x) : -(x))

keyboard_config_t keyboard_config;
uint16_t          dpi_array[]  = PLOOPY_DPI_OPTIONS;
float             scroll_div[] = PLOOPY_SCROLL_DIV_OPTIONS;
uint16_t          snipe_dpi    = PLOOPY_SNIPE_DPI;
#define DPI_OPTION_SIZE ARRAY_SIZE(dpi_array)
#define SCROLL_DIV_OPTION_SIZE ARRAY_SIZE(scroll_div)

// Defined variables
#ifdef PLOOPY_DRAGSCROLL_INVERT
const int16_t vscroll_sign = -1;
#else
const int16_t vscroll_sign = 1;
#endif
enum DRAG_SCROLL_PRIORITY { PRI_NONE, PRI_V, PRI_H };

// Trackball State
bool  is_sniping           = false;
#ifdef PLOOPY_SNIPE_MOMENTARY
bool  is_snipe_momentary   = true;
#else
bool  is_snipe_momentary   = false;
#endif
bool  is_scroll_clicked    = false;
bool  is_drag_scroll       = false;
bool  is_drag_scroll_snap  = true;
enum DRAG_SCROLL_PRIORITY drag_scroll_priority = PRI_NONE;
bool  is_hires_scroll = true;
float scroll_accumulated_h = 0;
float scroll_accumulated_v = 0;
uint32_t last_scroll_time = 0;

#ifdef PLOOPY_DRAGSCROLL_MOMENTARY
bool  is_drag_scroll_momentary  = true;
#else
bool  is_drag_scroll_momentary  = false;
#endif

// variables used by scroll-snapping
// the averages are EWMA.
// piggy-back on scroll_accumulated seems to be a bad idea
int16_t cumulated_delta_h = 0;
int16_t cumulated_delta_v = 0;
float average_scroll_vector_h = 0;
float average_scroll_vector_v = 0;
uint32_t last_snap_sample_time = 0;
float snap_wema_t_div = PLOOPY_SNAP_EWMA_T;
uint32_t snap_sample_period = PLOOPY_SNAP_EWMA_SMP_PERIOD;

#ifdef ENCODER_ENABLE
uint16_t lastScroll        = 0; // Previous confirmed wheel event
uint16_t lastMidClick      = 0; // Stops scrollwheel from being read if it was pressed
pin_t    encoder_pins_a[1] = ENCODER_A_PINS;
pin_t    encoder_pins_b[1] = ENCODER_B_PINS;
bool     debug_encoder     = false;

bool encoder_update_kb(uint8_t index, bool clockwise) {
    if (!encoder_update_user(index, clockwise)) {
        return false;
    }
#    ifdef MOUSEKEY_ENABLE
    tap_code(clockwise ? KC_WH_U : KC_WH_D);
#    else
    report_mouse_t mouse_report = pointing_device_get_report();
    mouse_report.v              = clockwise ? 1 : -1;
    pointing_device_set_report(mouse_report);
    pointing_device_send();
#    endif
    return true;
}

void encoder_driver_init(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(encoder_pins_a); i++) {
        gpio_set_pin_input(encoder_pins_a[i]);
        gpio_set_pin_input(encoder_pins_b[i]);
    }
    opt_encoder_init();
}

void encoder_driver_task(void) {
    uint16_t p1 = analogReadPin(encoder_pins_a[0]);
    uint16_t p2 = analogReadPin(encoder_pins_b[0]);

    if (debug_encoder) dprintf("OPT1: %d, OPT2: %d\n", p1, p2);

    int8_t dir = opt_encoder_handler(p1, p2);
    // If the mouse wheel was just released, do not scroll.
    if (timer_elapsed(lastMidClick) < PLOOPY_SCROLL_BUTTON_DEBOUNCE) {
        return;
    }

    // Limit the number of scrolls per unit time.
    if (timer_elapsed(lastScroll) < PLOOPY_SCROLL_DEBOUNCE) {
        return;
    }

    // Don't scroll if the middle button is depressed.
    if (is_scroll_clicked) {
#    ifndef PLOOPY_IGNORE_SCROLL_CLICK
        return;
#    endif
    }

    if (dir == 0) return;
    encoder_queue_event(0, dir > 0);
    lastScroll = timer_read();
}
#endif

void toggle_drag_scroll(void) {
    is_drag_scroll ^= 1;
}

void cycle_dpi(void) {
    keyboard_config.dpi_config = (keyboard_config.dpi_config + 1) % DPI_OPTION_SIZE;
    eeconfig_update_kb(keyboard_config.raw);
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

void cycle_scroll_priority() {
    if (drag_scroll_priority == PRI_NONE)
        drag_scroll_priority = PRI_V;
    else if (drag_scroll_priority == PRI_V)
        drag_scroll_priority = PRI_H;
    else
        drag_scroll_priority = PRI_NONE;
}

void toggle_scroll_snap() {
    is_drag_scroll_snap ^= 1;
}

void toggle_hires_scroll() {
    is_hires_scroll ^= 1;
}

void toggle_dragscroll_momentary() {
    if (is_drag_scroll_momentary) {
        is_drag_scroll_momentary = false;
        is_drag_scroll = true;
    } else {
        is_drag_scroll_momentary = true;
        is_drag_scroll = false;
    }
}

void cycle_scroll_div() {
    keyboard_config.scroll_div_config = (keyboard_config.scroll_div_config + 1) % SCROLL_DIV_OPTION_SIZE;
    eeconfig_update_kb(keyboard_config.raw);
}

void enable_snipe() {
    is_sniping = false;
    toggle_snipe_dpi();
}

void disable_snipe() {
    is_sniping = true;
    toggle_snipe_dpi();
}

void toggle_snipe_dpi() {
    if (is_sniping) {
        pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
        is_sniping = false;
    } else {
        pointing_device_set_cpi(snipe_dpi);
        is_sniping = true;
    }
}

void toggle_snipe_momentary() {
    is_snipe_momentary ^= 1;
}

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    static uint16_t hires_scroll_res = 1;

    if (!is_drag_scroll) {
        scroll_accumulated_h = scroll_accumulated_v = 0;
        cumulated_delta_h = cumulated_delta_v
            = average_scroll_vector_h = average_scroll_vector_v = 0;
        return pointing_device_task_user(mouse_report);
    }

    if (is_drag_scroll_snap) {
        // The elapsed time of first sample will be extremely large,
        // but it's probably fine.
        uint32_t snap_elapsed_time = timer_elapsed32(last_snap_sample_time);
        if (snap_elapsed_time < snap_sample_period) {
            cumulated_delta_h += mouse_report.x;
            cumulated_delta_v += mouse_report.y;
        } else {
            last_snap_sample_time = timer_read32();
            float alpha = expf(- (float)snap_elapsed_time / snap_wema_t_div);
            average_scroll_vector_h = alpha * average_scroll_vector_h
                                      + (1 - alpha) * abs((float)cumulated_delta_h) / snap_elapsed_time;
            average_scroll_vector_v = alpha * average_scroll_vector_v
                                      + (1 - alpha) * abs((float)cumulated_delta_v) / snap_elapsed_time;
            cumulated_delta_h = cumulated_delta_v = 0;
        }

        // SNAP, PRI_NONE is free scroll with snapping
        if (drag_scroll_priority == PRI_H
                || average_scroll_vector_h >= average_scroll_vector_v * PLOOPY_SNAP_RATIO) {
            scroll_accumulated_h += (float)mouse_report.x / scroll_div[keyboard_config.scroll_div_config];
        }
        if (drag_scroll_priority == PRI_V
                || average_scroll_vector_v >= average_scroll_vector_h * PLOOPY_SNAP_RATIO) {
            scroll_accumulated_v += (float)mouse_report.y / scroll_div[keyboard_config.scroll_div_config];
        }
    } else {
        // NO_SNAP, PRI_NONE is true free scroll
        if (drag_scroll_priority != PRI_V)
            scroll_accumulated_h += (float)mouse_report.x / scroll_div[keyboard_config.scroll_div_config];
        if (drag_scroll_priority != PRI_H)
            scroll_accumulated_v += (float)mouse_report.y / scroll_div[keyboard_config.scroll_div_config];
    }


    // throttle scrolling
    if (timer_elapsed32(last_scroll_time) < 16) {
        mouse_report.h = 0;
        mouse_report.v = 0;
    } else {
        last_scroll_time = timer_read32();

        if (is_hires_scroll) {
            // Assign integer parts of accumulated scroll values to the mouse report
            mouse_report.h = (int16_t) scroll_accumulated_h;
            mouse_report.v = (int16_t) (vscroll_sign * scroll_accumulated_v);
            // Update accumulated scroll values by subtracting the integer parts
            scroll_accumulated_h -= (int16_t)scroll_accumulated_h;
            scroll_accumulated_v -= (int16_t)scroll_accumulated_v;
        } else {
            // Shamelessly copied from https://github.com/adept-hires-scroll-mod/qmk_firmware
            // Emulate no hires scrolling by only reporting in increments of the resolution
            hires_scroll_res = pointing_device_get_hires_scroll_resolution();
            mouse_report.h = (int16_t) scroll_accumulated_h / hires_scroll_res * hires_scroll_res;
            mouse_report.v = (int16_t) scroll_accumulated_v / hires_scroll_res * hires_scroll_res;

            // In case vscroll sign is -1
            scroll_accumulated_v -= mouse_report.v;
            mouse_report.v *= vscroll_sign;

            scroll_accumulated_h -= mouse_report.h;
        }
    }

    // Clear the X and Y values of the mouse report
    mouse_report.x = 0;
    mouse_report.y = 0;

    return pointing_device_task_user(mouse_report);
}

bool process_record_kb(uint16_t keycode, keyrecord_t* record) {
    if (debug_mouse) {
        dprintf("KL: kc: %u, col: %u, row: %u, pressed: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed);
    }

    // Update Timer to prevent accidental scrolls
#ifdef ENCODER_ENABLE
    if ((record->event.key.col == ENCODER_BUTTON_COL) && (record->event.key.row == ENCODER_BUTTON_ROW)) {
        lastMidClick      = timer_read();
        is_scroll_clicked = record->event.pressed;
    }
#endif

    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (keycode == DPI_CONFIG && record->event.pressed) {
        cycle_dpi();
    }

    if (keycode == DRAG_SCROLL && is_drag_scroll_momentary) {
        is_drag_scroll = record->event.pressed;
    } else if (keycode == SNIPE_DPI && is_snipe_momentary) {
        if (record->event.pressed) {
            enable_snipe();
        } else {
            disable_snipe();
        }
    } else if (record->event.pressed) {
        switch (keycode) {
        case DRAG_SCROLL:
            toggle_drag_scroll();
            break;
        case SNIPE_DPI:
            toggle_snipe_dpi();
            break;
        case TOGGLE_DRAGSCROLL_MOMENTARY:
            toggle_dragscroll_momentary();
            break;
        case TOGGLE_SCROLL_SNAP:
            toggle_scroll_snap();
            break;
        case TOGGLE_HIRES_SCROLL:
            toggle_hires_scroll();
            break;
        case CYCLE_SCROLL_DIV:
            cycle_scroll_div();
            break;
        case CYCLE_SCROLL_PRIORITY:
            cycle_scroll_priority();
            break;
        case TOGGLE_SNIPE_MOMENTARY:
            toggle_snipe_momentary();
            break;
        }
    }

    return true;
}

// Hardware Setup
void keyboard_pre_init_kb(void) {
    // debug_enable  = true;
    // debug_matrix  = true;
    // debug_mouse   = true;
    // debug_encoder = true;

    /* Ground all output pins connected to ground. This provides additional
     * pathways to ground. If you're messing with this, know this: driving ANY
     * of these pins high will cause a short. On the MCU. Ka-blooey.
     */
#ifdef UNUSABLE_PINS
    const pin_t unused_pins[] = UNUSABLE_PINS;

    for (uint8_t i = 0; i < ARRAY_SIZE(unused_pins); i++) {
        gpio_set_pin_output_push_pull(unused_pins[i]);
        gpio_write_pin_low(unused_pins[i]);
    }
#endif

    // This is the debug LED.
#if defined(DEBUG_LED_PIN)
    gpio_set_pin_output_push_pull(DEBUG_LED_PIN);
    gpio_write_pin(DEBUG_LED_PIN, debug_enable);
#endif

    keyboard_pre_init_user();
}

void pointing_device_init_kb(void) {
    keyboard_config.raw = eeconfig_read_kb();
    if (keyboard_config.dpi_config > DPI_OPTION_SIZE
            || keyboard_config.scroll_div_config > SCROLL_DIV_OPTION_SIZE) {
        eeconfig_init_kb();
    }
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

void eeconfig_init_kb(void) {
    keyboard_config.dpi_config = PLOOPY_DPI_DEFAULT;
    keyboard_config.scroll_div_config = PLOOPY_SCROLL_DIV_DEFAULT;
    eeconfig_update_kb(keyboard_config.raw);
    eeconfig_init_user();
}
