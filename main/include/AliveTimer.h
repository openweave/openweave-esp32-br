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

#ifndef OPENWEAVE_ESP32_ALIVE_TIMER_H_
#define OPENWEAVE_ESP32_ALIVE_TIMER_H_

#include <Weave/DeviceLayer/WeaveDeviceLayer.h>

class AliveTimer {
public:
    static WEAVE_ERROR Start(uint32_t intervalMS);
private:
    static void HandleAliveTimer(::nl::Weave::System::Layer * /* unused */, void * /* unused */, ::nl::Weave::System::Error /* unused */);
    static uint32_t AliveIntervalMS;
};

#endif // OPENWEAVE_ESP32_ALIVE_TIMER_H_
