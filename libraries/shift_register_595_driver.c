#include "Shift_Register_595_Driver.h"

void shift_register_init(shift_register_595 *reg, int8_t ser_pin,
                         int8_t oe_pin, int8_t rclk_pin, int8_t srclk_pin, int8_t srclr_pin) {
    reg->SER_pin = ser_pin;
    reg->OE_pin = oe_pin;
    reg->RCLK_pin = rclk_pin;
    reg->SRCLK_pin = srclk_pin;
    reg->SRCLR_pin = srclr_pin;
}