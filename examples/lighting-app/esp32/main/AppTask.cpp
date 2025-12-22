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

#include "AppTask.h"
#include "Button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <app-common/zap-generated/attributes/Accessors.h>

#define APP_TASK_NAME "APP"
#define APP_EVENT_QUEUE_SIZE 10
#define APP_TASK_STACK_SIZE (3072)
#define BUTTON_PRESSED 1
#define APP_LIGHT_SWITCH 1

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::DeviceLayer;

static const char TAG[] = "APPTask";

LEDWidget AppLED;
Button AppButton;

namespace {
constexpr EndpointId kLightEndpointId = 1;
QueueHandle_t sAppEventQueue;
TaskHandle_t sAppTaskHandle;
} // namespace

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::StartAppTask()
{
    sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
    if (sAppEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate app event queue");
        return APP_ERROR_EVENT_QUEUE_FAILED;
    }

    // Start App task.
    BaseType_t xReturned;
    xReturned = xTaskCreate(AppTaskMain, APP_TASK_NAME, APP_TASK_STACK_SIZE, NULL, 1, &sAppTaskHandle);
    return (xReturned == pdPASS) ? CHIP_NO_ERROR : APP_ERROR_CREATE_TASK_FAILED;
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;

    AppButton.Init();
    AppLED.Init();
    ESP_LOGI(TAG, "App Task started");

    while (true)
    {
        BaseType_t eventReceived = xQueueReceive(sAppEventQueue, &event, pdMS_TO_TICKS(10));
        while (eventReceived == pdTRUE)
        {
            if (event.mHandler)
            {
                event.mHandler(&event);
            }
            eventReceived = xQueueReceive(sAppEventQueue, &event, 0); // return immediately if the queue is empty
        }
    }
}

void AppTask::ButtonEventHandler()
{
    AppEvent button_event = {};
    button_event.Type     = AppEvent::kEventType_Button;
    button_event.mHandler = AppTask::LightingActionEventHandler;
    if (xPortInIsrContext())
    {
        BaseType_t higherPrioTaskWoken = pdFALSE;
        xQueueSendFromISR(sAppEventQueue, &button_event, &higherPrioTaskWoken);
    }
    else
    {
        xQueueSend(sAppEventQueue, &button_event, 1);
    }
}

void AppTask::LightingActionEventHandler(AppEvent * aEvent)
{
    ESP_LOGI(TAG, "button pressed");

    AppLED.Toggle();
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    sAppTask.UpdateClusterState();
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

void AppTask::UpdateClusterState()
{
    ESP_LOGI(TAG, "Writing to OnOff cluster");
    // write the new on/off value
    Protocols::InteractionModel::Status status = Clusters::OnOff::Attributes::OnOff::Set(kLightEndpointId, AppLED.IsTurnedOn());

    if (status != Protocols::InteractionModel::Status::Success)
    {
        ESP_LOGE(TAG, "Updating on/off cluster failed: %x", to_underlying(status));
    }

    ESP_LOGI(TAG, "Writing to Current Level cluster");
    status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kLightEndpointId, AppLED.GetLevel());

    if (status != Protocols::InteractionModel::Status::Success)
    {
        ESP_LOGE(TAG, "Updating level cluster failed: %x", to_underlying(status));
    }
}
