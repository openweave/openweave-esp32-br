/*
 *
 *    Copyright 2020 Google LLC
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

#ifndef OPENWEAVE_ESP32_BUTTON_H_
#define OPENWEAVE_ESP32_BUTTON_H_

#include <esp_system.h>
#include <esp_log.h>
#include <driver/gpio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Button
{
public:
    esp_err_t Init(gpio_num_t gpioNum, uint16_t debouncePeriod);
    bool Poll();
    uint32_t GetStateDuration();
    bool IsPressed() { return mState; }

private:
    uint32_t mLastReadTime;     // in ticks
    uint32_t mStateStartTime;   // in ticks
    uint32_t mPrevStateDur;     // in ticks
    gpio_num_t mGPIONum;
    uint16_t mDebouncePeriod;   // in ticks
    bool mState;
    bool mLastState;
};

#endif // OPENWEAVE_ESP32_BUTTON_H_
