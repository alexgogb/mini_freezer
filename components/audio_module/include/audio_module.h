/*
 * author: Alejandro González Blanco (alexgogb)
 *
 */

#pragma once

#include <stdint.h>
#include "hal/ledc_types.h"

typedef struct {
    ledc_channel_t ledc_channel;
    ledc_mode_t ledc_mode;
    int8_t audio_pin;
} audio_device;

void audio_device_init(audio_device *ad, ledc_channel_t channel, ledc_mode_t mode, int8_t pin);
void audio_device_send_pulse(audio_device ad, int32_t duration);
void audio_device_warning(audio_device ad);
void audio_device_turn_off(audio_device ad);