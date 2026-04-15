/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "../components/LCD_1602_driver/include/LCD_1602_driver.h"
#include "../components/shift_register_595_driver/include/shift_register_595_driver.h"
#include "../components/audio_module/include/audio_module.h"

#define SER 4
#define RCLK 5
#define SRCLK 6
#define AUDIO 7
#define DOOR_WARNING 10

#define LEDC_DUTY_RESOLUTION LEDC_TIMER_10_BIT  // (0-1023)
#define LEDC_AUDIO_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_FREQUENCY 1000              // 1 kHz

void initial_config();
void audioDriverTask(void *pvParameters);

sr_595 shift_register;
LCD_1602 lcd;
audio_device ad;
volatile uint8_t warning_on = 0;

void app_main(void) {
    initial_config();

    sr_init(&shift_register, SER, GPIO_NUM_NC, RCLK, SRCLK, GPIO_NUM_NC);
    LCD_init(&lcd, MODE_4_BIT, &shift_register);
    audio_device_init(&ad, LEDC_AUDIO_CHANNEL, LEDC_MODE, AUDIO);
    
    // LCD_write_line(lcd, "Hola");
    // vTaskDelay(pdMS_TO_TICKS(5000));
    // LCD_clear(lcd);
    // vTaskDelay(pdMS_TO_TICKS(5000));
    // LCD_write_line(lcd, "Adios");

    // LCD_switch_to_second_line(lcd, 0);
    // LCD_write_line(lcd, "Segunda linea");
    // LCD_cursor_off_blink_off(lcd);

    // while (1);

    // audio_device_warning(ad);
    // audio_device_send_pulse(ad, 3000);

    while (1) {
        if (gpio_get_level(10)) {
            warning_on = 1;
        } else {
            warning_on = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initial_config() {
    gpio_config_t io_config_out = {
        .pin_bit_mask = (1ULL << SER) | (1ULL << RCLK) | (1ULL << SRCLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config_out);

    gpio_config_t io_config_in = {
        .pin_bit_mask = (1ULL << DOOR_WARNING),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config_in);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RESOLUTION,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = AUDIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_AUDIO_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0
    };
    ledc_channel_config(&ledc_channel);

    xTaskCreate(audioDriverTask, "Audio task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}

void audioDriverTask(void *pvParameters) {
    while (1) {
        if (warning_on) {
            audio_device_warning(ad);
        } else {
            audio_device_turn_off(ad);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}