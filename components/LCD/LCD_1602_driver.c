/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include "LCD_1602_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void LCD_init(LCD_1602 *lcd, uint8_t mode, sr_595 *reg) {
    lcd->mode = mode;
    lcd->shift_register = reg;

    vTaskDelay(pdMS_TO_TICKS(50));
    // First 0x30
    LCD_write_command(*lcd, 0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    // Second 0x30
    LCD_write_command(*lcd, 0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    // Third 0x30
    LCD_write_command(*lcd, 0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    // 0x20 to set to 4-bit mode
    LCD_write_command(*lcd, 0x20);

    // 0x28 to set 4-bit/2-line
    LCD_write_command(*lcd, 0x20);
    LCD_write_command(*lcd, 0x80);

    // 0x10 to set cursor
    LCD_write_command(*lcd, 0x10);
    LCD_write_command(*lcd, 0x00);

    // 0x0F to turn display on
    LCD_write_command(*lcd, 0x00);
    LCD_write_command(*lcd, 0xF0);

    // 0x06 to turn display on
    LCD_write_command(*lcd, 0x00);
    LCD_write_command(*lcd, 0x60);
}

void LCD_write_command(LCD_1602 lcd, uint8_t command) {
    sr_write(*lcd.shift_register, command | 0x8);
    vTaskDelay(pdMS_TO_TICKS(1));
    sr_write(*lcd.shift_register, command & 0xF7);
}