/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include "Shift_Register_595_Driver.h"

void sr_init(sr_595 *reg, int8_t ser_pin, int8_t oe_pin, 
             int8_t rclk_pin, int8_t srclk_pin, int8_t srclr_pin) {
    reg->SER_pin = ser_pin;
    reg->OE_pin = oe_pin;
    reg->RCLK_pin = rclk_pin;
    reg->SRCLK_pin = srclk_pin;
    reg->SRCLR_pin = srclr_pin;
}

/*
 * Most significant bit will be in QH.
 * Least significant bit will be in QA.
 */
void sr_write(sr_595 reg, int8_t value) {
    int i;

    for (i = 0; i < 8; ++i) {
        gpio_set_level(reg.SER_pin, (value << i) & 0x80);
        gpio_set_level(reg.SRCLK_pin, 1);
        gpio_set_level(reg.SRCLK_pin, 0);
    }
    
    gpio_set_level(reg.OE_pin, 0);
    gpio_set_level(reg.OE_pin, 1);
}