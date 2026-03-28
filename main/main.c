#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "../components/LCD_1602_driver/include/LCD_1602_driver.h"
#include "../components/shift_register_595_driver/include/shift_register_595_driver.h"

#define SER 4
#define RCLK 5
#define SRCLK 6

void initial_config();

void app_main(void) {
    sr_595 shift_register;
    LCD_1602 lcd;

    initial_config();

    sr_init(&shift_register, SER, GPIO_NUM_NC, RCLK, SRCLK, GPIO_NUM_NC);
    LCD_init(&lcd, MODE_4_BIT, &shift_register);
    
    LCD_write_line(lcd, "Hola");
    vTaskDelay(pdMS_TO_TICKS(5000));
    LCD_clear(lcd);
    vTaskDelay(pdMS_TO_TICKS(5000));
    LCD_write_line(lcd, "Adios");

    LCD_switch_to_second_line(lcd);
    LCD_write_line(lcd, "Segunda linea");
    LCD_cursor_off_blink_off(lcd);

    while (1);
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