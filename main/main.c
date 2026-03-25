#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void initial_config();
void set_LCD_display(uint8_t *string);

void app_main(void) {
    initial_config();
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