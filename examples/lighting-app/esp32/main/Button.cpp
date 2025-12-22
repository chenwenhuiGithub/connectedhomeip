/*
 *
 *    Copyright (c) 2022-2023 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"

#include "AppTask.h"
#include "Button.h"
#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceLayer.h>
#include <vector>

static const char TAG[] = "Button";
static TickType_t s_last_button_time = 0;
static const uint32_t DEBOUNCE_DELAY_MS = 50;

Button::Button() {}

void IRAM_ATTR button_isr_handler(void * arg)
{
    TickType_t now = xTaskGetTickCountFromISR();
    if ((now - s_last_button_time) > pdMS_TO_TICKS(DEBOUNCE_DELAY_MS)) {
        s_last_button_time = now;
        GetAppTask().ButtonEventHandler();
    }
}

esp_err_t Button::Init()
{
    gpio_config_t io_conf = {};

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = 1ULL << CONFIG_BUTTON_GPIO_NUM;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)CONFIG_BUTTON_GPIO_NUM, button_isr_handler, NULL);

    return ESP_OK;
}
