/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include <stdint.h>

#define MODE_4_BIT 4
#define MODE_8_BIT 8

typedef struct {
    uint8_t mode;
    uint8_t RS_pin;
    uint8_t RW_pin;
    uint8_t E_pin;
    uint8_t D0_pin, D1_pin, D2_pin, D3_pin,
            D4_pin, D5_pin, D6_pin, D7_pin;
} LCD_1602;

void LCD_init(LCD_1602 lcd);