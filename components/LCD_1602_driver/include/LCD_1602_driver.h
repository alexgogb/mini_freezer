/*
 * author: Alejandro González Blanco (alexgogb)
 *
 * This library uses a 595 shift register to write values.
 * 
 * Currently this library only supports 4-bit mode and the specific
 * features that are implemented here.
 */

#pragma once

#include <stdint.h>
#include "../../shift_register_595_driver/include/shift_register_595_driver.h"

#define MODE_4_BIT 4
#define MODE_8_BIT 8

// TO DO: add struct with row and column information to LCD_1602.

typedef struct {
    uint8_t mode;
    uint8_t line;

    sr_595 *shift_register;
} LCD_1602;

void LCD_init(LCD_1602 *lcd, uint8_t mode, sr_595 *reg);
void LCD_write_command(LCD_1602 lcd, uint8_t command);
void LCD_write_command_nibble(LCD_1602 lcd, uint8_t nibble);
void LCD_write_character(LCD_1602 lcd, uint8_t character);
void LCD_write_line(LCD_1602 lcd, char *string);
void LCD_switch_to_first_line(LCD_1602 lcd, uint8_t column);
void LCD_switch_to_second_line(LCD_1602 lcd, uint8_t column);
void LCD_clear(LCD_1602 lcd);
void LCD_turn_display_off(LCD_1602 lcd);
void LCD_cursor_on_blink_on(LCD_1602 lcd);
void LCD_cursor_off_blink_off(LCD_1602 lcd);