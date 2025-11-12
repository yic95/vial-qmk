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

#pragma once

#include "quantum.h"

typedef union {
    uint32_t raw;
    struct {
        uint8_t dpi_config;
        uint8_t scroll_div_config;
    };
} keyboard_config_t;

extern keyboard_config_t keyboard_config;
extern uint16_t          dpi_array[];
extern float             scroll_div[];

enum ploopy_keycodes {
    DPI_CONFIG = QK_KB_0,
    DRAG_SCROLL,
    TOGGLE_DRAGSCROLL_MOMENTARY,
    TOGGLE_SCROLL_SNAP,
    TOGGLE_HIRES_SCROLL,
    CYCLE_SCROLL_PRIORITY,
    CYCLE_SCROLL_DIV,
    SNIPE_DPI,
    TOGGLE_SNIPE_MOMENTARY
};

bool encoder_update_user(uint8_t index, bool clockwise);
bool encoder_update_kb(uint8_t index, bool clockwise);
void toggle_drag_scroll(void);
void cycle_dpi(void);
void toggle_hires_scroll(void);
void toggle_scroll_snap(void);
void toggle_dragscroll_momentary(void);
void cycle_scroll_priority(void);
void cycle_scroll_div(void);
void toggle_snipe_dpi(void);
void enable_snipe(void);
void disable_snipe(void);
void toggle_snipe_momentary(void);
