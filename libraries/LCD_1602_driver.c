/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include "LCD_1602_driver.h"

void LCD_init(LCD_1602 *lcd, uint8_t mode, uint8_t rs_pin, uint8_t rw_pin, uint8_t e_pin) {
    lcd->mode = mode;
    lcd->RS_pin = rs_pin;
    lcd->RW_pin = rw_pin;
    lcd->E_pin = e_pin;
}