/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include "include/LCD_1602_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void LCD_init(LCD_1602 *lcd, uint8_t mode, sr_595 *reg) {
    lcd->mode = mode;
    lcd->shift_register = reg;

    vTaskDelay(pdMS_TO_TICKS(100));

    // First 0x30
    LCD_write_command(*lcd, 0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    // Second 0x30
    LCD_write_command(*lcd, 0x30);
    // Third 0x30
    LCD_write_command(*lcd, 0x30);

    // 0x20 to set to 4-bit mode
    LCD_write_command(*lcd, 0x20);

    // 0x28 to set 4-bit/2-line
    LCD_write_command(*lcd, 0x20);
    LCD_write_command(*lcd, 0x80);

    // 0x08 to turn display and cursor OFF
    LCD_write_command(*lcd, 0x00);
    LCD_write_command(*lcd, 0x80);

    // 0x01 to clear display
    LCD_write_command(*lcd, 0x00);
    LCD_write_command(*lcd, 0x10);
    vTaskDelay(pdMS_TO_TICKS(3));

    // 0x06 to entry mode set
    LCD_write_command(*lcd, 0x00);
    LCD_write_command(*lcd, 0x60);

    // 0x0F to turn display ON
    LCD_write_command(*lcd, 0x00);
    LCD_write_command(*lcd, 0xF0);
}

void LCD_write_command(LCD_1602 lcd, uint8_t command) {
    sr_write(*lcd.shift_register, command | 0x8); // Enable
    sr_write(*lcd.shift_register, command & 0xF7);
    vTaskDelay(pdMS_TO_TICKS(1));
}