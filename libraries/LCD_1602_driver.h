/*
 * author: Alejandro González Blanco (alexgogb)
 *
 * Currently this library only supports 4-bit mode and the specific
 * features that are implemented here.
 */

#include <stdint.h>

#define MODE_4_BIT 4
#define MODE_8_BIT 8

typedef struct {
    // User-defined
    uint8_t mode;
    uint8_t RS_pin;
    uint8_t RW_pin;
    uint8_t E_pin;

    // Useful for the developer
    uint8_t line;
} LCD_1602;

void LCD_init(LCD_1602 *lcd, uint8_t mode, uint8_t rs_pin, uint8_t rw_pin, uint8_t e_pin);
void LCD_write_character(LCD_1602 lcd, uint8_t character);
void LCD_write_line(LCD_1602 lcd, uint8_t *string);
void LCD_change_line(LCD_1602 lcd);
void LCD_clear(LCD_1602 lcd);