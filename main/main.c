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
#define PIN_SER 4
#define PIN_RCLK 5
#define PIN_SRCLK 6
#define PIN_AUDIO 7
#define PIN_DOOR_WARNING 10
#define PIN_PELTIER 11
#define PIN_FAN -
#define PIN_BUTTON_SUM 21
#define PIN_BUTTON_MIN 22
#define PIN_BUTTON_OK 23

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
void button_sum_isr(void *args);
void button_min_isr(void *args);
void button_ok_isr(void *args);

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
uint8_t peltier_current_mode = 0;

portMUX_TYPE mutex_door_warning = portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t button_queue;
QueueHandle_t peltier_result_queue;
EventGroupHandle_t button_event_group;
TickType_t button_last_time[3] = {0, 0, 0};

void app_main(void) {
    esp32_initial_config();

    sr_init(&shift_register, PIN_SER, GPIO_NUM_NC, PIN_RCLK, PIN_SRCLK, GPIO_NUM_NC);
    LCD_init(&lcd, MODE_4_BIT, &shift_register);
    audio_device_init(&ad, LEDC_AUDIO_CHANNEL, LEDC_MODE, PIN_AUDIO);
    
    LCD_write_line(lcd, "Bienvenido");
    LCD_cursor_off_blink_off(lcd);
    vTaskDelay(pdMS_TO_TICKS(1500));
    LCD_clear(lcd);
    
    while (1) {
        // Peltier selection window start.
        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(peltier_result_queue, &peltier_current_mode, portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);
        // Peltier selection window end.

        if (peltier_current_mode != 0) {
            ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 1023 / peltier_current_mode);
        } else {
            ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
        }

        ledc_update_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void esp32_initial_config() {
    gpio_config_t io_config_out = {
        .pin_bit_mask = (1ULL << PIN_SER) | (1ULL << PIN_RCLK) | (1ULL << PIN_SRCLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config_out);

    gpio_config_t io_config_in = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_SUM) | (1ULL << PIN_BUTTON_MIN) | (1ULL << PIN_BUTTON_OK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
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
        .gpio_num = PIN_AUDIO,
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
        .gpio_num = PIN_PELTIER,
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

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_21, button_sum_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_22, button_min_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_23, button_ok_isr, NULL);

    button_event_group = xEventGroupCreate();
    button_queue = xQueueCreate(10, sizeof(button_event));
    peltier_result_queue = xQueueCreate(1, sizeof(uint8_t));
    xTaskCreate(button_handler_task, "Button task", 2048, NULL, 3, NULL);
    xTaskCreate(audio_driver_task, "Audio task", 2048, NULL, tskIDLE_PRIORITY, NULL);
}

void door_open_safety_system() {
    warning_on++;
}

void button_handler_task(void *args) {
    uint8_t peltier_selection_mode = 0;
    button_event event;
    TickType_t button_now;

    while (1) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            button_now = xTaskGetTickCount();

            if ((button_now - button_last_time[event]) < pdMS_TO_TICKS(100)) {
                continue;
            }
            button_last_time[event] = button_now;

            if (!(xEventGroupGetBits(button_event_group) & BIT0)) {
                audio_device_send_pulse(ad, 100);
                continue;
            }

            switch (event) {
                case BUTTON_SUM:
                    if (peltier_selection_mode < 4) {
                        peltier_selection_mode++;
                    }
                    audio_device_send_pulse(ad, 100);
                    break;
                case BUTTON_MIN:
                    if (peltier_selection_mode > 0) {
                        peltier_selection_mode--;
                    }
                    audio_device_send_pulse(ad, 100);
                    break;
                case BUTTON_OK:
                    xQueueSend(peltier_result_queue, &peltier_selection_mode, 0);
                    audio_device_send_pulse(ad, 100);
                    // peltier_selection_mode = 0;
                    break;
            }
        }
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
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void IRAM_ATTR button_sum_isr(void *args) {
    button_event event = BUTTON_SUM;
    xQueueSendFromISR(button_queue, &event, NULL);
}

void IRAM_ATTR button_min_isr(void *args) {
    button_event event = BUTTON_MIN;
    xQueueSendFromISR(button_queue, &event, NULL);
}

void IRAM_ATTR button_ok_isr(void *args) {
    button_event event = BUTTON_OK;
    xQueueSendFromISR(button_queue, &event, NULL);
}