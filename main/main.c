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
#define PIN_LIGHT 3
#define PIN_SER 4
#define PIN_RCLK 5
#define PIN_SRCLK 6
#define PIN_AUDIO 7
#define PIN_DOOR_WARNING 10
#define PIN_PELTIER 11
#define PIN_FAN 18
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
#define LEDC_FAN_CHANNEL LEDC_CHANNEL_2
#define LEDC_FAN_TIMER   LEDC_TIMER_1
#define LEDC_FAN_FREQUENCY 25000

#define HOURS 0
#define MINUTES 1
#define SECONDS 2

void esp32_initial_config();
void button_handler_task(void *args);
void audio_driver_task(void *args);
void door_open_timer_callback(void *args);
void fan_timer_callback(void *args);
void button_sum_isr(void *args);
void button_min_isr(void *args);
void button_ok_isr(void *args);
void door_open_isr(void *args);
void door_close_isr(void *args);
void door_isr(void *args);
void safe_shutdown();
void write_time(char *string, time_selection_state state);
void update_time_display(time_selection_state state);

typedef enum {
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_OK
} button_event;

typedef enum {
    AUDIO_BEEP,
    AUDIO_WARNING
} audio_event;

typedef enum {
    MODE_SELECTION,
    TIME_SELECTION,
    WORKING
} system_state;

typedef enum {
    SELECTING_HOURS,
    SELECTING_MINUTES,
    SELECTING_SECONDS
} time_selection_state;

sr_595 shift_register;
LCD_1602 lcd;
audio_device ad;

esp_timer_handle_t peltier_timer;
esp_timer_handle_t door_open_timer;
esp_timer_handle_t fan_timer;

uint8_t system_current_state;
volatile uint8_t warning_on = 0;
uint8_t peltier_current_mode = 0;
uint8_t peltier_time[3] = {0, 0, 0};
volatile uint8_t door_open = 0;
volatile int64_t remaining_us = 0;
volatile uint8_t door_timer_phase = 0;
int64_t start_time_us = 0;

time_selection_state time_state;

QueueHandle_t button_queue;
QueueHandle_t peltier_result_queue;
QueueHandle_t audio_queue;
QueueHandle_t time_queue;
EventGroupHandle_t button_event_group;
TickType_t button_last_time[3] = {0, 0, 0};

void app_main(void) {
    int64_t hours;
    int64_t minutes;
    int64_t seconds;
    uint64_t total_us;
    esp32_initial_config();

    sr_init(&shift_register, PIN_SER, GPIO_NUM_NC, PIN_RCLK, PIN_SRCLK, GPIO_NUM_NC);
    LCD_init(&lcd, MODE_4_BIT, &shift_register);
    audio_device_init(&ad, LEDC_AUDIO_CHANNEL, LEDC_MODE, PIN_AUDIO);
    
    LCD_write_line(lcd, "Bienvenido");
    LCD_cursor_off_blink_off(lcd);
    vTaskDelay(pdMS_TO_TICKS(1500));
    LCD_clear(lcd);
    
    while (1) {
        system_current_state = MODE_SELECTION;
        LCD_write_line(lcd, "Seleccione modo:");
        LCD_switch_to_second_line(lcd, 0);
        LCD_write_character(lcd, '-');
        LCD_switch_to_second_line(lcd, '+');
        LCD_switch_to_second_line(lcd, 5);
        // Peltier selection window start.
        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(peltier_result_queue, &peltier_current_mode, portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);
        // Peltier selection window end.

        system_current_state = TIME_SELECTION;
        time_state = SELECTING_HOURS;
        LCD_clear(lcd);
        LCD_write_line(lcd, "Tiempo de uso:");
        LCD_switch_to_second_line(lcd, 0);
        write_time("00:00:00", time_state);
        // Hours window.
        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(time_queue, &peltier_time[HOURS], portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);
        // Minutes window.
        time_state = SELECTING_MINUTES;
        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(time_queue, &peltier_time[MINUTES], portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);
        // Seconds window.
        time_state = SELECTING_SECONDS;
        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(time_queue, &peltier_time[SECONDS], portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);

        ledc_set_duty(LEDC_MODE, LEDC_FAN_CHANNEL, 1023);
        ledc_update_duty(LEDC_MODE, LEDC_FAN_CHANNEL);

        if (peltier_current_mode != 0) {
            ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 1023 / peltier_current_mode);
        } else {
            ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
        }
        ledc_update_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL);

        total_us = ((uint64_t)peltier_time[HOURS]   * 3600
                   + (uint64_t)peltier_time[MINUTES] * 60
                   + (uint64_t)peltier_time[SECONDS]) * 1000000ULL;

        remaining_us = (int64_t)total_us;
        start_time_us = esp_timer_get_time();
        system_current_state = WORKING;

        esp_timer_start_once(peltier_timer, total_us);

        LCD_clear(lcd);
        LCD_write_line(lcd, "Enfriando...");

        while (system_current_state == WORKING) {
            if (!door_open) {
                int64_t current_time = esp_timer_get_time();
                int64_t time_left = remaining_us - (current_time - start_time_us);

                if (time_left < 0) {
                    time_left = 0;
                }

                hours = time_left / 3600000000LL;
                minutes = (time_left % 3600000000LL) / 60000000LL;
                seconds = (time_left % 60000000LL) / 1000000LL;

                char time_str[9];
                snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hours, minutes, seconds);

                LCD_switch_to_second_line(lcd, 0);
                LCD_write_line(lcd, time_str);
            } else {
                LCD_write_line(lcd, "Door open!");
                // Second line shows frozen time.
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }

        LCD_clear(lcd);
        if (warning_on) {
            LCD_write_line(lcd, "Apagado de");
            LCD_switch_to_second_line(lcd, 0);
            LCD_write_line(lcd, "seguridad");
            warning_on = 0;
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            LCD_write_line(lcd, "Fin del");
            LCD_switch_to_second_line(lcd, 0);
            LCD_write_line(lcd, "enfriamiento");
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}

void esp32_initial_config() {
    gpio_config_t io_config_out = {
        .pin_bit_mask = (1ULL << PIN_SER) | (1ULL << PIN_RCLK) | (1ULL << PIN_SRCLK) | (1ULL << PIN_LIGHT),
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

    gpio_config_t io_door = {
        .pin_bit_mask = (1ULL << PIN_DOOR_WARNING),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_door);

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

    ledc_timer_config_t ledc_fan_timer = {
    .speed_mode = LEDC_MODE,
    .duty_resolution = LEDC_DUTY_RESOLUTION,
    .timer_num = LEDC_FAN_TIMER,
    .freq_hz = LEDC_FAN_FREQUENCY
    };
    ledc_timer_config(&ledc_fan_timer);

    ledc_channel_config_t ledc_fan_channel = {
        .gpio_num = PIN_FAN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_FAN_CHANNEL,
        .timer_sel = LEDC_FAN_TIMER,
        .duty = 0
    };
    ledc_channel_config(&ledc_fan_channel);

    esp_timer_create_args_t peltier_timer_args = {
        .callback = &safe_shutdown,
        .arg = NULL,
        .name = "Peltier timer"
    };
    esp_timer_create(&peltier_timer_args, &peltier_timer);

    esp_timer_create_args_t door_timer_args = {
        .callback = &door_open_timer_callback,
        .arg = NULL,
        .name = "Door open timer"
    };
    esp_timer_create(&door_timer_args, &door_open_timer);

    esp_timer_create_args_t fan_timer_args = {
    .callback = &fan_timer_callback,
    .arg = NULL,
    .name = "Fan timer"
    };
    esp_timer_create(&fan_timer_args, &fan_timer);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_10, door_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_21, button_sum_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_22, button_min_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_23, button_ok_isr, NULL);

    button_event_group = xEventGroupCreate();
    button_queue = xQueueCreate(10, sizeof(button_event));
    peltier_result_queue = xQueueCreate(1, sizeof(uint8_t));
    audio_queue = xQueueCreate(5, sizeof(audio_event));
    time_queue = xQueueCreate(5, sizeof(uint8_t));
    xTaskCreate(button_handler_task, "Button task", 2048, NULL, 3, NULL);
    xTaskCreate(audio_driver_task, "Audio task", 2048, NULL, tskIDLE_PRIORITY, NULL);
}

void button_handler_task(void *args) {
    uint8_t peltier_selection_mode = 0;
    uint8_t peltier_selection_time = 0;
    button_event event;
    audio_event audio;
    TickType_t button_now;

    while (1) {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY)) {
            button_now = xTaskGetTickCount();

            if ((button_now - button_last_time[event]) < pdMS_TO_TICKS(100)) {
                continue;
            }
            button_last_time[event] = button_now;

            audio = AUDIO_BEEP;
            xQueueSend(audio_queue, &audio, 0);

            if (!(xEventGroupGetBits(button_event_group) & BIT0)) {
                continue;
            }

            switch (event) {
                case BUTTON_LEFT:
                    if (system_current_state == MODE_SELECTION) {
                        if (peltier_selection_mode < 4) {
                            peltier_selection_mode++;
                            LCD_write_character(lcd, 0xFF);
                        }
                    } else if (system_current_state == TIME_SELECTION) {
                        if (peltier_selection_time > 0) {
                            peltier_selection_time--;
                        } else {
                            peltier_selection_time = 59;
                        }
                        peltier_time[time_state] = peltier_selection_time;
                        update_time_display(time_state);
                    }
                    
                    break;
                case BUTTON_RIGHT:
                    if (system_current_state == MODE_SELECTION) {
                        if (peltier_selection_mode > 0) {
                            peltier_selection_mode--;
                            LCD_write_command(lcd, 0x10); // Move left
                            LCD_write_character(lcd, 0x20);
                            LCD_write_command(lcd, 0x10);
                        }
                    } else if (system_current_state == TIME_SELECTION) {
                        if (peltier_selection_time < 59) {
                            peltier_selection_time++;
                        } else {
                            peltier_selection_time = 0;
                        }
                        peltier_time[time_state] = peltier_selection_time;
                        update_time_display(time_state);
                    }
                    
                    break;
                case BUTTON_OK:
                    if (system_current_state == MODE_SELECTION) {
                        xQueueSend(peltier_result_queue, &peltier_selection_mode, 0);
                        peltier_selection_mode = 0;
                    } else if (system_current_state == TIME_SELECTION) {
                        xQueueSend(time_queue, &peltier_selection_time, 0);
                        peltier_selection_time = 0;
                    } else if (system_current_state == WORKING) {
                        safe_shutdown();
                    }
                    
                    break;
            }
        }
    }
}

void audio_driver_task(void *args) {
    audio_event event;

    while (1) {
        if (xQueueReceive(audio_queue, &event, portMAX_DELAY)) {
            audio_device_turn_off(ad);

            switch (event) {
                case AUDIO_BEEP:
                    audio_device_send_pulse(ad, 50);
                    break;
                case AUDIO_WARNING:
                    audio_device_warning(ad);
                    break;
            }
        }
    }
}

void door_open_timer_callback(void *args) {
    if (door_timer_phase == 0) {
        // 15s: alarm.
        door_timer_phase = 1;
        warning_on = 1;
        gpio_set_level(PIN_LIGHT, 1);

        audio_event audio = AUDIO_WARNING;
        xQueueSendFromISR(audio_queue, &audio, NULL);

        // + 25s (40s for complete shutdown)
        esp_timer_start_once(door_open_timer, 25 * 1000000ULL);
    } else {
        // 40s: safety_system
        if (system_current_state == WORKING) {
            safe_shutdown();
        }
    }
}

void fan_timer_callback(void *args) {
    ledc_set_duty(LEDC_MODE, LEDC_FAN_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_FAN_CHANNEL);
}

void IRAM_ATTR button_sum_isr(void *args) {
    button_event event = BUTTON_LEFT;
    xQueueSendFromISR(button_queue, &event, NULL);
}

void IRAM_ATTR button_min_isr(void *args) {
    button_event event = BUTTON_RIGHT;
    xQueueSendFromISR(button_queue, &event, NULL);
}

void IRAM_ATTR button_ok_isr(void *args) {
    button_event event = BUTTON_OK;
    xQueueSendFromISR(button_queue, &event, NULL);
}

void IRAM_ATTR door_open_isr(void *args) {
    if (system_current_state == WORKING) {
        // Stop peltier timer (while the door is open it does not work)
        int64_t now = esp_timer_get_time();
        remaining_us -= (now - start_time_us);
        esp_timer_stop(peltier_timer);
    }

    gpio_set_level(PIN_LIGHT, 1);

    door_open = 1;
    door_timer_phase = 0;
    esp_timer_start_once(door_open_timer, 15 * 1000000ULL);
}

void IRAM_ATTR door_close_isr(void *args) {
    esp_timer_stop(door_open_timer);
    warning_on = 0;
    door_open = 0;

    gpio_set_level(PIN_LIGHT, 0);

    audio_event audio = AUDIO_BEEP;
    xQueueSendFromISR(audio_queue, &audio, NULL);

    if (system_current_state == WORKING && remaining_us > 0) {
        start_time_us = esp_timer_get_time();
        esp_timer_start_once(peltier_timer, remaining_us);
    }
}

void IRAM_ATTR door_isr(void *args) {
    if (gpio_get_level(PIN_DOOR_WARNING) == 1) {
        door_open_isr(NULL);
    } else {
        door_close_isr(NULL);
    }
}

void safe_shutdown() {
    esp_timer_stop(peltier_timer);
    ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL);
    system_current_state = MODE_SELECTION;

    esp_timer_start_once(fan_timer, 15 * 1000000ULL);
}

// Always writes to second line.
void write_time(char *string, time_selection_state state) {
    LCD_switch_to_second_line(lcd, 0);
    LCD_write_line(lcd, string);

    if (state == SELECTING_HOURS) {
        LCD_switch_to_second_line(lcd, 1);
        LCD_cursor_on_blink_on(lcd);
    } else if (state == SELECTING_MINUTES) {
        LCD_switch_to_second_line(lcd, 4);
        LCD_cursor_on_blink_on(lcd);
    } else if (state == SELECTING_SECONDS) {
        LCD_switch_to_second_line(lcd, 7);
        LCD_cursor_on_blink_on(lcd);
    }
}

void update_time_display(time_selection_state state) {
    char time_str[9]; // "HH:MM:SS\0"
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             peltier_time[HOURS],
             peltier_time[MINUTES],
             peltier_time[SECONDS]);
    write_time(time_str, state);
}