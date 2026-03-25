#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "../libraries/shift_register_595_driver.h"
#include "../libraries/LCD_1602_driver.h"

#define SER 4
#define OE 5
#define SRCLK 6

void initial_config();

void app_main(void) {
    sr_595 shift_register;

    initial_config();

    sr_init(&shift_register, SER, OE, GPIO_NUM_NC, SRCLK, GPIO_NUM_NC);
}

void initial_config() {
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << 4) | (1ULL << 5) | (1ULL << 6),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config);
}