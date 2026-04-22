/*
 * author: Alejandro González Blanco (alexgogb)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "hal/ledc_types.h"
#include "../components/LCD_1602_driver/include/LCD_1602_driver.h"
#include "../components/shift_register_595_driver/include/shift_register_595_driver.h"
#include "../components/audio_module/include/audio_module.h"

// GPIOs 2, 3, 4, 5, 6, 7, 10, 11, 18, 19, 20, 21, 22, 23: usable.
#define SER 4
#define RCLK 5
#define SRCLK 6
#define AUDIO 7
#define DOOR_WARNING 10
#define PELTIER 11
#define FAN - 

#define BUTTON_DEBOUNCE_COUNT 5

#define LEDC_DUTY_RESOLUTION LEDC_TIMER_10_BIT  // (0-1023)
#define LEDC_AUDIO_CHANNEL LEDC_CHANNEL_0
#define LEDC_AUDIO_TIMER LEDC_TIMER_0
#define LEDC_PELTIER_CHANNEL LEDC_CHANNEL_1
#define LEDC_PELTIER_TIMER LEDC_TIMER_2
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_AUDIO_FREQUENCY 1000 // 1 kHz
#define LEDC_PELTIER_FREQUENCY 4000

void esp32_initial_config();
void door_open_safety_system();
void button_handler_task(void *args);
void audio_driver_task(void *args);

typedef enum {
    BUTTON_SUM,
    BUTTON_MIN,
    BUTTON_OK
} button_event;

sr_595 shift_register;
LCD_1602 lcd;
audio_device ad;
esp_timer_handle_t timer;
volatile uint8_t warning_on = 0;
volatile uint8_t door_warning = 0;
uint8_t button_counter = 0;
uint8_t button_last_read = 1;
uint8_t button_state = 1;
uint8_t peltier_mode = 0;
portMUX_TYPE mutex_door_warning = portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t button_queue;

void app_main(void) {
    uint8_t button_current;
    esp32_initial_config();

    sr_init(&shift_register, SER, GPIO_NUM_NC, RCLK, SRCLK, GPIO_NUM_NC);
    LCD_init(&lcd, MODE_4_BIT, &shift_register);
    audio_device_init(&ad, LEDC_AUDIO_CHANNEL, LEDC_MODE, AUDIO);
    
    LCD_write_line(lcd, "Bienvenido");
    LCD_cursor_off_blink_off(lcd);
    vTaskDelay(pdMS_TO_TICKS(1500));
    LCD_clear(lcd);

    // esp_timer_start_once(timer, 10 * 1000 * 1000);

    // if (door_warning) {
    //     gpio_set_level(SER, 1);
    //     door_warning = 0;
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    //     gpio_set_level(SER, 0);
    //     esp_timer_stop(timer);
    //     esp_timer_start_once(timer, 5 * 1000 * 1000);
    // }
    // vTaskDelay(pdMS_TO_TICKS(10));
    
    while (1) {
        button_current = gpio_get_level(DOOR_WARNING);

        if (button_current == button_last_read) {
            button_counter++;

            if (button_counter >= BUTTON_DEBOUNCE_COUNT) {
                if (button_state != button_current) {
                    button_state = button_current;

                    if (button_state) {
                        if (peltier_mode < 4) {
                            peltier_mode++;
                        } else {
                            peltier_mode = 0;
                        }

                        if (peltier_mode != 0) {
                            ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 1023 / peltier_mode);
                        } else {
                            ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
                        }

                        ledc_update_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL);
                    }
                }
            }
        } else {
            button_counter = 0;
        }

        button_last_read = button_current;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void esp32_initial_config() {
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
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config_in);

    ledc_timer_config_t ledc_audio_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RESOLUTION,
        .timer_num = LEDC_AUDIO_TIMER,
        .freq_hz = LEDC_AUDIO_FREQUENCY
    };
    ledc_timer_config(&ledc_audio_timer);

    ledc_channel_config_t ledc_audio_channel = {
        .gpio_num = AUDIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_AUDIO_CHANNEL,
        .timer_sel = LEDC_AUDIO_TIMER,
        .duty = 0
    };
    ledc_channel_config(&ledc_audio_channel);

    ledc_timer_config_t ledc_peltier_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RESOLUTION,
        .timer_num = LEDC_PELTIER_TIMER,
        .freq_hz = LEDC_PELTIER_FREQUENCY
    };
    ledc_timer_config(&ledc_peltier_timer);

    ledc_channel_config_t ledc_peltier_channel = {
        .gpio_num = PELTIER,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_PELTIER_CHANNEL,
        .timer_sel = LEDC_PELTIER_TIMER,
        .duty = 0
    };
    ledc_channel_config(&ledc_peltier_channel);

    esp_timer_create_args_t timer_args = {
        .callback = &door_open_safety_system,
        .arg = NULL,
        .name = "timer"
    };
    esp_timer_create(&timer_args, &timer);

    button_queue = xQueueCreate(10, sizeof(button_event));
    xTaskCreate(button_handler_task, "Button task", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(audio_driver_task, "Audio task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}

void door_open_safety_system() {
    door_warning++;
}

void button_handler_task(void *args) {
    button_event event;

    while (1) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            switch (event) {
                case BUTTON_SUM:
                    if (peltier_mode < 4) {
                        peltier_mode++;
                    }
                    break;
                case BUTTON_MIN:
                    if (peltier_mode > 0) {
                        peltier_mode--;
                    }
                    break;
                case BUTTON_OK:
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void audio_driver_task(void *args) {
    while (1) {
        taskENTER_CRITICAL(&mutex_door_warning);
        uint8_t read_warning_on = warning_on;
        taskEXIT_CRITICAL(&mutex_door_warning);

        if (read_warning_on) {
            audio_device_warning(ad);
        } else {
            audio_device_turn_off(ad);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}