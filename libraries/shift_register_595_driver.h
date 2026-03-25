/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include <stdint.h>

typedef struct {
    int8_t QA_pin, QB_pin, QC_pin, QD_pin,
            QE_pin, QF_pin, QG_pin, QH_pin, QHp_pin;
    int8_t SER_pin;
    int8_t OE_pin;
    int8_t RCLK_pin;
    int8_t SRCLK_pin;
    int8_t SRCLR_pin;
} shift_register_595;

void shift_register_init(shift_register_595 *reg, int8_t *data_pins, int8_t ser_pin,
                         int8_t oe_pin, int8_t rclk_pin, int8_t srclk_pin, int8_t srclr_pin);