/*
 * author: Alejandro González Blanco (alexgogb)
 *
 * Currently this library only supports a very specific case where the least
 * amount of pins of the shift register are used.
 * 
 */

#include <stdint.h>

typedef struct {
    int8_t SER_pin;
    int8_t OE_pin;
    int8_t RCLK_pin;
    int8_t SRCLK_pin;
    int8_t SRCLR_pin;
} sr_595;

void sr_init(sr_595 *reg, int8_t ser_pin, int8_t oe_pin,
             int8_t rclk_pin, int8_t srclk_pin, int8_t srclr_pin);

void sr_write(sr_595 reg, int8_t value);
