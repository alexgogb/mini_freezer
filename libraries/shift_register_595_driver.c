#include "Shift_Register_595_Driver.h"

void shift_register_init(shift_register_595 *reg, int8_t *data_pins, int8_t ser_pin,
                         int8_t oe_pin, int8_t rclk_pin, int8_t srclk_pin, int8_t srclr_pin) {
    reg->QHp_pin = data_pins[8];
    reg->QH_pin = data_pins[7];
    reg->QG_pin = data_pins[6];
    reg->QF_pin = data_pins[5];
    reg->QE_pin = data_pins[4];
    reg->QD_pin = data_pins[3];
    reg->QC_pin = data_pins[2];
    reg->QB_pin = data_pins[1];
    reg->QA_pin = data_pins[0];

    reg->SER_pin = ser_pin;
    reg->OE_pin = oe_pin;
    reg->RCLK_pin = rclk_pin;
    reg->SRCLK_pin = srclk_pin;
    reg->SRCLR_pin = srclr_pin;
}