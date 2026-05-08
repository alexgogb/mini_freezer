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
#include "../components/audio_module/include/audio_module.h"
#include "dht.h"
#include "hd44780.h"
#include "pcf8574.h"
#include "i2cdev.h"
#include "string.h"

// GPIOs 2, 3, 4, 5, 6, 7, 10, 11, 18, 19, 20, 21, 22, 23: usable.
#define PIN_SDA 4
#define PIN_SCL 5
#define PIN_LIGHT 6
#define PIN_AUDIO 7
#define PIN_DOOR_WARNING 10
#define PIN_PELTIER 11
#define PIN_FAN 18
#define PIN_DHT 19
#define PIN_AUXILIAR_FAN 20
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

#define HOURS 0
#define MINUTES 1
#define SECONDS 2

#define DHT_TYPE DHT_TYPE_DHT11

#define I2C_PORT I2C_NUM_0
#define LCD_I2C_ADDR 0x27

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

typedef enum {
    DOOR_EVENT_OPEN,
    DOOR_EVENT_CLOSE
} door_event_t;


typedef enum {
    SHUTDOWN_NONE,
    SHUTDOWN_NORMAL,
    SHUTDOWN_SAFETY,
    SHUTDOWN_USER
} shutdown_reason_t;


typedef struct {
    int16_t temperature;
    int16_t humidity;
    bool valid;
} sensor_data_t;

void esp32_initial_config(void);
void button_handler_task(void *args);
void audio_driver_task(void *args);
void dht_task(void *args);
void door_task(void *args);

void door_open_timer_callback(void *args);
void fan_timer_callback(void *args);

void button_sum_isr(void *args);
void button_min_isr(void *args);
void button_ok_isr(void *args);
void door_isr(void *args);


void safe_shutdown(shutdown_reason_t reason);
void peltier_timer_callback(void *args);

void write_time(char *string, time_selection_state state);
void update_time_display(time_selection_state state);

void lcd_puts_padded_16(uint8_t col, uint8_t row, const char *text);
void lcd_clear_safe(void);
void lcd_lock(void);
void lcd_unlock(void);

static esp_err_t write_lcd_data(const hd44780_t *lcd, uint8_t data);

audio_device ad;
static i2c_dev_t pcf8574;

hd44780_t lcd = {
    .write_cb = write_lcd_data,
    .font = HD44780_FONT_5X8,
    .lines = 2,
    .pins = {
        .rs = 0,
        .e  = 2,
        .d4 = 4,
        .d5 = 5,
        .d6 = 6,
        .d7 = 7,
        .bl = 3
    }
};

esp_timer_handle_t peltier_timer;
esp_timer_handle_t door_open_timer;
esp_timer_handle_t fan_timer;

volatile system_state system_current_state = MODE_SELECTION;
volatile uint8_t warning_on = 0;
uint8_t peltier_current_mode = 0;
uint8_t peltier_time[3] = {0, 0, 0};

volatile uint8_t door_open = 0;
volatile int64_t remaining_us = 0;
volatile uint8_t door_timer_phase = 0;
volatile shutdown_reason_t shutdown_reason = SHUTDOWN_NONE;
int64_t start_time_us = 0;

time_selection_state time_state;

QueueHandle_t button_queue;
QueueHandle_t peltier_result_queue;
QueueHandle_t audio_queue;
QueueHandle_t time_queue;
QueueHandle_t door_queue;

EventGroupHandle_t button_event_group;

SemaphoreHandle_t lcd_mutex;
SemaphoreHandle_t sensor_mutex;

sensor_data_t sensor_data = {
    .temperature = 0,
    .humidity = 0,
    .valid = false
};

TickType_t button_last_time[3] = {0, 0, 0};
volatile TickType_t door_last_time = 0;

static portMUX_TYPE shutdown_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool shutdown_in_progress = false;

void app_main(void) {
    int64_t hours;
    int64_t minutes;
    int64_t seconds;
    int64_t current_time;
    int64_t time_left;
    uint64_t total_us;

    char status_string[20];
    char time_str[20] = "00:00:00";

    esp32_initial_config();

    audio_device_init(&ad, LEDC_AUDIO_CHANNEL, LEDC_MODE, PIN_AUDIO);

    while (1) {
        shutdown_in_progress = false;
        shutdown_reason = SHUTDOWN_NONE;

        door_open = 0;
        warning_on = 0;
        door_timer_phase = 0;

        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        xQueueReset(peltier_result_queue);
        xQueueReset(time_queue);

        audio_device_turn_off(ad);

        lcd_clear_safe();

        lcd_lock();
        hd44780_control(&lcd, true, false, false);
        lcd_puts_padded_16(0, 0, "Bienvenido");
        lcd_puts_padded_16(0, 1, "");
        lcd_unlock();

        vTaskDelay(pdMS_TO_TICKS(1500));

        lcd_clear_safe();


        system_current_state = MODE_SELECTION;

        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        xQueueReset(peltier_result_queue);
        audio_device_turn_off(ad);

        lcd_clear_safe();

        lcd_lock();
        lcd_puts_padded_16(0, 0, "Seleccione modo:");
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_putc(&lcd, '-');
        hd44780_gotoxy(&lcd, 15, 1);
        hd44780_putc(&lcd, '+');
        hd44780_gotoxy(&lcd, 5, 1);
        lcd_unlock();

        // Peltier selection window start.
        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(peltier_result_queue, &peltier_current_mode, portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);

        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        audio_device_turn_off(ad);
        // Peltier selection window end.

        system_current_state = TIME_SELECTION;
        time_state = SELECTING_HOURS;

        peltier_time[HOURS] = 0;
        peltier_time[MINUTES] = 0;
        peltier_time[SECONDS] = 0;

        lcd_clear_safe();

        lcd_lock();
        lcd_puts_padded_16(0, 0, "Tiempo de uso:");
        lcd_unlock();

        write_time("00:00:00", time_state);

        // Hours window.
        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        xQueueReset(time_queue);
        audio_device_turn_off(ad);

        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(time_queue, &peltier_time[HOURS], portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);

        // Minutes window.
        time_state = SELECTING_MINUTES;

        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        audio_device_turn_off(ad);

        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(time_queue, &peltier_time[MINUTES], portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);

        // Seconds window.
        time_state = SELECTING_SECONDS;

        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        audio_device_turn_off(ad);

        xEventGroupSetBits(button_event_group, BIT0);
        xQueueReceive(time_queue, &peltier_time[SECONDS], portMAX_DELAY);
        xEventGroupClearBits(button_event_group, BIT0);

        xQueueReset(button_queue);
        xQueueReset(audio_queue);
        audio_device_turn_off(ad);

        esp_timer_stop(fan_timer);

        gpio_set_level(PIN_FAN, 1);
        gpio_set_level(PIN_AUXILIAR_FAN, 1);

        vTaskDelay(pdMS_TO_TICKS(500));

        lcd_lock();
        hd44780_init(&lcd);
        hd44780_switch_backlight(&lcd, true);
        hd44780_control(&lcd, true, false, false);
        lcd_unlock();

        switch (peltier_current_mode) {
            case 0:
                ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
                break;

            case 1:
                ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 760);
                break;

            case 2:
                ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 511);
                break;

            case 3:
                ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 341);
                break;

            case 4:
                ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 255);
                break;

            default:
                ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
                break;
        }

        ledc_update_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL);

        total_us = ((uint64_t)peltier_time[HOURS] * 3600ULL
                  + (uint64_t)peltier_time[MINUTES] * 60ULL
                  + (uint64_t)peltier_time[SECONDS]) * 1000000ULL;

        remaining_us = (int64_t)total_us;
        start_time_us = esp_timer_get_time();
        system_current_state = WORKING;

        if (total_us > 0) {
            if (gpio_get_level(PIN_DOOR_WARNING)) {
                door_open = 1;
                gpio_set_level(PIN_LIGHT, 1);

                door_timer_phase = 0;
                esp_timer_stop(door_open_timer);
                esp_timer_start_once(door_open_timer, 15 * 1000000ULL);
            } else {
                door_open = 0;
                gpio_set_level(PIN_LIGHT, 0);

                esp_timer_start_once(peltier_timer, total_us);
            }
        } else {
            safe_shutdown(SHUTDOWN_NORMAL);
        }

        lcd_clear_safe();

        lcd_lock();
        lcd_puts_padded_16(0, 0, "Enfriando...");
        lcd_unlock();

        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
            peltier_time[HOURS],
            peltier_time[MINUTES],
            peltier_time[SECONDS]);

        while (system_current_state == WORKING) {
            if (!door_open) {
                current_time = esp_timer_get_time();
                time_left = remaining_us - (current_time - start_time_us);

                if (time_left < 0) {
                    time_left = 0;
                }

                hours = time_left / 3600000000LL;
                minutes = (time_left % 3600000000LL) / 60000000LL;
                seconds = (time_left % 60000000LL) / 1000000LL;

                snprintf(time_str, sizeof(time_str), "%02lld:%02lld:%02lld",
                         hours, minutes, seconds);
            }

            sensor_data_t local_sensor = {
                .temperature = 0,
                .humidity = 0,
                .valid = false
            };

            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                local_sensor = sensor_data;
                xSemaphoreGive(sensor_mutex);
            }

            if (local_sensor.valid) {
                snprintf(status_string, sizeof(status_string), "T:%2d%cC H:%2d%%",
                         local_sensor.temperature / 10,
                         0xDF,
                         local_sensor.humidity / 10);
            } else {
                snprintf(status_string, sizeof(status_string), "T:--%cC H:--%%", 0xDF);
            }

            lcd_lock();

            if (door_open) {
                lcd_puts_padded_16(0, 0, "Puerta abierta");
                lcd_puts_padded_16(0, 1, time_str);
            } else {
                lcd_puts_padded_16(0, 0, status_string);
                lcd_puts_padded_16(0, 1, time_str);
            }

            lcd_unlock();

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        lcd_clear_safe();

        lcd_lock();
        hd44780_control(&lcd, true, false, false);

        if (shutdown_reason == SHUTDOWN_SAFETY) {
            lcd_puts_padded_16(0, 0, "Apagado de");
            lcd_puts_padded_16(0, 1, "seguridad");
        } else if (shutdown_reason == SHUTDOWN_USER) {
            lcd_puts_padded_16(0, 0, "Cancelado por");
            lcd_puts_padded_16(0, 1, "usuario");
        } else {
            lcd_puts_padded_16(0, 0, "Fin del");
            lcd_puts_padded_16(0, 1, "enfriamiento");
        }

        lcd_unlock();

        vTaskDelay(pdMS_TO_TICKS(1800));

        lcd_clear_safe();   
    }
}

void esp32_initial_config(void) {
    ESP_ERROR_CHECK(i2cdev_init());

    memset(&pcf8574, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(pcf8574_init_desc(&pcf8574, LCD_I2C_ADDR, I2C_NUM_0, PIN_SDA, PIN_SCL));

    lcd_mutex = xSemaphoreCreateMutex();
    sensor_mutex = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(hd44780_init(&lcd));

    lcd_lock();
    hd44780_switch_backlight(&lcd, true);
    lcd_unlock();

    gpio_config_t io_config_out = {
        .pin_bit_mask = (1ULL << PIN_LIGHT) |
                        (1ULL << PIN_FAN) |
                        (1ULL << PIN_AUXILIAR_FAN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config_out);

    gpio_config_t io_config_in = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_SUM) |
                        (1ULL << PIN_BUTTON_MIN) |
                        (1ULL << PIN_BUTTON_OK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_config_in);

    gpio_config_t io_config_dht = {
        .pin_bit_mask = (1ULL << PIN_DHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config_dht);

    gpio_config_t io_config_door = {
        .pin_bit_mask = (1ULL << PIN_DOOR_WARNING),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_config_door);

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

    esp_timer_create_args_t peltier_timer_args = {
        .callback = &peltier_timer_callback,
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

    button_event_group = xEventGroupCreate();

    button_queue = xQueueCreate(10, sizeof(button_event));
    peltier_result_queue = xQueueCreate(1, sizeof(uint8_t));
    audio_queue = xQueueCreate(5, sizeof(audio_event));
    time_queue = xQueueCreate(5, sizeof(uint8_t));
    door_queue = xQueueCreate(5, sizeof(door_event_t));

    gpio_install_isr_service(0);

    gpio_isr_handler_add(GPIO_NUM_10, door_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_21, button_sum_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_22, button_min_isr, NULL);
    gpio_isr_handler_add(GPIO_NUM_23, button_ok_isr, NULL);

    xTaskCreate(button_handler_task, "Button task", 3072, NULL, 3, NULL);
    xTaskCreate(audio_driver_task, "Audio task", 2048, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(dht_task, "DHT task", 4096, NULL, 5, NULL);
    xTaskCreate(door_task, "Door task", 4096, NULL, 4, NULL);
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

            EventBits_t bits = xEventGroupGetBits(button_event_group);

            bool selection_active = (bits & BIT0) != 0;
            bool working_ok = (system_current_state == WORKING && event == BUTTON_OK);

            if (!selection_active && !working_ok) {
                continue;
            }

            audio = AUDIO_BEEP;
            xQueueSend(audio_queue, &audio, 0);

            switch (event) {
                case BUTTON_LEFT:
                    if (system_current_state == MODE_SELECTION) {
                        if (peltier_selection_mode > 0) {
                            peltier_selection_mode--;

                            lcd_lock();

                            switch (peltier_selection_mode) {
                                case 0:
                                    hd44780_gotoxy(&lcd, 5, 1);
                                    hd44780_putc(&lcd, 0x20);
                                    hd44780_gotoxy(&lcd, 5, 1);
                                    break;

                                case 1:
                                    hd44780_gotoxy(&lcd, 6, 1);
                                    hd44780_putc(&lcd, 0x20);
                                    hd44780_gotoxy(&lcd, 6, 1);
                                    break;

                                case 2:
                                    hd44780_gotoxy(&lcd, 7, 1);
                                    hd44780_putc(&lcd, 0x20);
                                    hd44780_gotoxy(&lcd, 7, 1);
                                    break;

                                case 3:
                                    hd44780_gotoxy(&lcd, 8, 1);
                                    hd44780_putc(&lcd, 0x20);
                                    hd44780_gotoxy(&lcd, 8, 1);
                                    break;

                                default:
                                    break;
                            }

                            lcd_unlock();
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
                        if (peltier_selection_mode < 4) {
                            peltier_selection_mode++;

                            lcd_lock();
                            hd44780_putc(&lcd, 0xFF);
                            lcd_unlock();
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
                        safe_shutdown(SHUTDOWN_USER);
                    }

                    break;

                default:
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

                default:
                    break;
            }
        }
    }
}

void dht_task(void *args) {
    int16_t temperature = 0;
    int16_t humidity = 0;

    while (1) {
        esp_err_t res = dht_read_data(DHT_TYPE, PIN_DHT, &humidity, &temperature);

        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (res == ESP_OK) {
                sensor_data.temperature = temperature;
                sensor_data.humidity = humidity;
                sensor_data.valid = true;
            } else {
                sensor_data.valid = false;
            }

            xSemaphoreGive(sensor_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void door_task(void *args) {
    door_event_t event;

    while (1) {
        if (xQueueReceive(door_queue, &event, portMAX_DELAY)) {
            if (event == DOOR_EVENT_OPEN) {
                gpio_set_level(PIN_LIGHT, 1);
                door_open = 1;

                if (system_current_state == WORKING && !shutdown_in_progress) {
                    int64_t now = esp_timer_get_time();

                    remaining_us -= (now - start_time_us);

                    if (remaining_us < 0) {
                        remaining_us = 0;
                    }

                    esp_timer_stop(peltier_timer);

                    door_timer_phase = 0;

                    esp_timer_stop(door_open_timer);
                    esp_timer_start_once(door_open_timer, 15 * 1000000ULL);
                } else {
                    esp_timer_stop(door_open_timer);
                    door_timer_phase = 0;
                }

            } else {
                esp_timer_stop(door_open_timer);

                warning_on = 0;
                door_open = 0;
                door_timer_phase = 0;

                gpio_set_level(PIN_LIGHT, 0);

                audio_device_turn_off(ad);
                xQueueReset(audio_queue);

                if (system_current_state == WORKING &&
                    remaining_us > 0 &&
                    !shutdown_in_progress) {

                    start_time_us = esp_timer_get_time();

                    esp_timer_start_once(peltier_timer, remaining_us);
                }
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
        xQueueSend(audio_queue, &audio, 0);

        // +25s; 40s total hasta apagado de seguridad.
        esp_timer_start_once(door_open_timer, 25 * 1000000ULL);
    } else {
        // 40s: safety shutdown.
        if (system_current_state == WORKING) {
            safe_shutdown(SHUTDOWN_SAFETY);
        }
    }
}

void fan_timer_callback(void *args) {
    gpio_set_level(PIN_FAN, 0);
    gpio_set_level(PIN_AUXILIAR_FAN, 0);
}

void IRAM_ATTR button_sum_isr(void *args) {
    button_event event = BUTTON_RIGHT;
    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(button_queue, &event, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR button_min_isr(void *args) {
    button_event event = BUTTON_LEFT;
    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(button_queue, &event, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR button_ok_isr(void *args) {
    button_event event = BUTTON_OK;
    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(button_queue, &event, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR door_isr(void *args) {
    TickType_t door_current_time = xTaskGetTickCountFromISR();

    if ((door_current_time - door_last_time) < pdMS_TO_TICKS(50)) {
        return;
    }

    door_last_time = door_current_time;

    door_event_t event;

    if (gpio_get_level(PIN_DOOR_WARNING)) {
        event = DOOR_EVENT_OPEN;
    } else {
        event = DOOR_EVENT_CLOSE;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(door_queue, &event, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void safe_shutdown(shutdown_reason_t reason) {
    portENTER_CRITICAL(&shutdown_mux);

    if (shutdown_in_progress) {
        portEXIT_CRITICAL(&shutdown_mux);
        return;
    }

    shutdown_in_progress = true;
    shutdown_reason = reason;

    portEXIT_CRITICAL(&shutdown_mux);

    esp_timer_stop(peltier_timer);
    esp_timer_stop(door_open_timer);

    ledc_set_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_PELTIER_CHANNEL);

    audio_device_turn_off(ad);

    if (audio_queue != NULL) {
        xQueueReset(audio_queue);
    }

    warning_on = 0;
    door_open = 0;
    door_timer_phase = 0;

    gpio_set_level(PIN_LIGHT, 0);

    system_current_state = MODE_SELECTION;

    esp_timer_stop(fan_timer);
    esp_timer_start_once(fan_timer, 15 * 1000000ULL);
}

void peltier_timer_callback(void *args) {
    safe_shutdown(SHUTDOWN_NORMAL);
}

// Always writes to second line.
void write_time(char *string, time_selection_state state) {
    lcd_lock();

    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, string);

    if (state == SELECTING_HOURS) {
        hd44780_gotoxy(&lcd, 1, 1);
        hd44780_control(&lcd, true, true, true);
    } else if (state == SELECTING_MINUTES) {
        hd44780_gotoxy(&lcd, 4, 1);
        hd44780_control(&lcd, true, true, true);
    } else if (state == SELECTING_SECONDS) {
        hd44780_gotoxy(&lcd, 7, 1);
        hd44780_control(&lcd, true, true, true);
    }

    lcd_unlock();
}

void update_time_display(time_selection_state state) {
    char time_str[12]; // "HH:MM:SS\0"

    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             peltier_time[HOURS],
             peltier_time[MINUTES],
             peltier_time[SECONDS]);

    write_time(time_str, state);
}

void lcd_puts_padded_16(uint8_t col, uint8_t row, const char *text) {
    char line[17];

    snprintf(line, sizeof(line), "%-16.16s", text);

    hd44780_gotoxy(&lcd, col, row);
    hd44780_puts(&lcd, line);
}

void lcd_clear_safe(void) {
    lcd_lock();
    hd44780_clear(&lcd);
    lcd_unlock();
}

void lcd_lock(void) {
    if (lcd_mutex != NULL) {
        xSemaphoreTake(lcd_mutex, portMAX_DELAY);
    }
}

void lcd_unlock(void) {
    if (lcd_mutex != NULL) {
        xSemaphoreGive(lcd_mutex);
    }
}

static esp_err_t write_lcd_data(const hd44780_t *lcd, uint8_t data) {
    return pcf8574_port_write(&pcf8574, data);
}