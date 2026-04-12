#include "audio_module.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

void audio_device_init(audio_device *ad, ledc_channel_t channel, ledc_mode_t mode, int8_t pin) {
    ad->ledc_channel = channel;
    ad->ledc_mode = mode;
    ad->audio_pin = pin;
}

void audio_device_send_pulse(audio_device ad, int32_t duration_ms) {
    ledc_set_duty(ad.ledc_mode, ad.ledc_channel, 512);
    ledc_update_duty(ad.ledc_mode, ad.ledc_channel);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(ad.ledc_mode, ad.ledc_channel, 0);
    ledc_update_duty(ad.ledc_mode, ad.ledc_channel);
}

void audio_device_warning(audio_device ad) {
    for (int duty = 0; duty <= 1023; duty += 10) {
        ledc_set_duty(ad.ledc_mode, ad.ledc_channel, duty);
        ledc_update_duty(ad.ledc_mode, ad.ledc_channel);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (int duty = 1023; duty >= 0; duty -= 10) {
        ledc_set_duty(ad.ledc_mode, ad.ledc_channel, duty);
        ledc_update_duty(ad.ledc_mode, ad.ledc_channel);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void audio_device_turn_off(audio_device ad) {
    ledc_set_duty(ad.ledc_mode, ad.ledc_channel, 0);
    ledc_update_duty(ad.ledc_mode, ad.ledc_channel);
}