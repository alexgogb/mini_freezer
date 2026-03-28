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
    LCD_write_command_nibble(*lcd, 0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    // Second 0x30
    LCD_write_command_nibble(*lcd, 0x30);
    // Third 0x30
    LCD_write_command_nibble(*lcd, 0x30);

    // 0x20 to set to 4-bit mode
    LCD_write_command_nibble(*lcd, 0x20);

    // 0x28 to set 4-bit/2-line
    LCD_write_command(*lcd, 0x28);

    LCD_turn_display_off(*lcd);

    LCD_clear(*lcd);

    // 0x06 to entry mode set
    LCD_write_command(*lcd, 0x06);

    LCD_cursor_on_blink_on(*lcd);
}

void LCD_write_command(LCD_1602 lcd, uint8_t command) {
    uint8_t upper_command = command & 0xF0;
    uint8_t lower_command = command << 4;

    sr_write(*lcd.shift_register, upper_command | 0x8); // Enable
    sr_write(*lcd.shift_register, upper_command & 0xF7); // Disable 
    sr_write(*lcd.shift_register, lower_command | 0x8);
    sr_write(*lcd.shift_register, lower_command & 0xF7);

    vTaskDelay(pdMS_TO_TICKS(1));
}

void LCD_write_command_nibble(LCD_1602 lcd, uint8_t nibble) {
    sr_write(*lcd.shift_register, nibble | 0x8); // Enable
    sr_write(*lcd.shift_register, nibble & 0xF7); // Disable 
    vTaskDelay(pdMS_TO_TICKS(1));
}

void LCD_write_character(LCD_1602 lcd, uint8_t character) {
    uint8_t upper_char = character & 0xF0;
    uint8_t lower_char = character << 4;

    sr_write(*lcd.shift_register, upper_char | 0xA); // Enable + RS
    sr_write(*lcd.shift_register, upper_char & 0xF5); // Disable 
    sr_write(*lcd.shift_register, lower_char | 0xA);
    sr_write(*lcd.shift_register, lower_char & 0xF5);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void LCD_write_line(LCD_1602 lcd, char *string) {
    while (*string) {
        LCD_write_character(lcd, *string);
        ++string;;
    }
}

void LCD_change_line(LCD_1602 lcd) {
    LCD_write_command(lcd, 0xC0);
}

void LCD_clear(LCD_1602 lcd) {
    // 0x01 to clear display
    LCD_write_command(lcd, 0x01);
    vTaskDelay(pdMS_TO_TICKS(3));
}

void LCD_turn_display_off(LCD_1602 lcd) {
    // 0x08 to turn display and cursor OFF
    LCD_write_command(lcd, 0x08);
}

void LCD_cursor_on_blink_on(LCD_1602 lcd) {
    // 0x0F to turn display ON
    LCD_write_command(lcd, 0x0F);
}

void LCD_cursor_off_blink_off(LCD_1602 lcd) {
    LCD_write_command(lcd, 0x0C);
}