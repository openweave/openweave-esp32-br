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

/*
 *   Description:
 *     OpenWeave ESP32 demo application.
 */

#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_heap_caps_init.h>
#include <new>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <Weave/DeviceLayer/WeaveDeviceLayer.h>
#include <Weave/Support/ErrorStr.h>

#include <openthread/openthread-esp32.h>
#include <openthread/cli.h>

#include "AliveTimer.h"
#include "Button.h"

using namespace ::nl;
using namespace ::nl::Inet;
using namespace ::nl::Weave;
using namespace ::nl::Weave::DeviceLayer;

const char * TAG = "openweave-esp32-br";

#if CONFIG_DEVICE_TYPE_M5STACK

#define ATTENTION_BUTTON_GPIO_NUM GPIO_NUM_37               // Use the right button (button "C") as the attention button on M5Stack

#elif CONFIG_DEVICE_TYPE_ESP32_DEVKITC

#define ATTENTION_BUTTON_GPIO_NUM GPIO_NUM_0                // Use the IO0 button as the attention button on ESP32-DevKitC and compatibles

#else // !CONFIG_DEVICE_TYPE_ESP32_DEVKITC

#error "Unsupported device type selected"

#endif // !CONFIG_DEVICE_TYPE_ESP32_DEVKITC

namespace nl {
namespace Weave {
namespace Profiles {
namespace DataManagement_Current {
namespace Platform {

void CriticalSectionEnter(void)
{
    return ;
}

void CriticalSectionExit(void)
{
    return ;
}

} // namespace Platform
} // namespace DataManagement_Current
} // namespace Profiles
} // namespace Weave
} // namespace nl

namespace {
Button sAttentionButton;

const uint32_t kSignalThreadInitDone = 1ul;

/* Handle events from the Weave Device layer.
 *
 * NOTE: This function runs on the Weave event loop task.
 */
void DeviceEventHandler(const WeaveDeviceEvent * event, intptr_t arg)
{
    // Just do nothing
}

void thread_init_task(void * arg)
{
    TaskHandle_t *called_task = (TaskHandle_t *) arg;
    otInstance *instance;
    WEAVE_ERROR err;
    
    otSysInit(0, NULL);
    size_t instanceSize = 0;
    // Get the instance size.
    otInstanceInit(NULL, &instanceSize);
    void *instanceBuffer = malloc(instanceSize);

    instance = otInstanceInit(instanceBuffer, &instanceSize);
    assert(instance != NULL);
 
    otCliUartInit(instance);

    ESP_LOGI(TAG, "Init OpenThread Stack");
    err = ThreadStackMgrImpl().InitThreadStack(instance);
    if (err != WEAVE_NO_ERROR) {
        ESP_LOGE(TAG, "ThreadStackMgr().InitThreadStack() failed: %s", ErrorStr(err));
        return;
    }

    ESP_LOGI(TAG, "Starting OpenThread Task");
    err = ThreadStackMgr().StartThreadTask();
    if (err != WEAVE_NO_ERROR) {
        ESP_LOGE(TAG, "ThreadStackMgr().StartThreadStack() failed: %s", ErrorStr(err));
        return;
    }

    // Notify main task to continue
    xTaskNotify(*called_task, kSignalThreadInitDone, eSetBits );
    vTaskDelete(NULL);
}
} // anonymous namespace

// OpenThread Init Requires huge stack size, since we are init precedure,
// let's create a new task and wait it.
void ot_init()
{
    TaskHandle_t xHandle = NULL;
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    BaseType_t res;
    res = xTaskCreate(thread_init_task,
                "otinit",
                13720 / sizeof(StackType_t),
                &current_task,
                5,
                &xHandle);
    assert(res == pdPASS);
    
    // Wait until thread_init_task finish
    uint32_t ulNotifiedValue = 0;
    while (!(ulNotifiedValue & kSignalThreadInitDone))
    {
        xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
    }
}

extern "C" void app_main()
{
    WEAVE_ERROR err;    // A quick note about errors: Weave adopts the error type and numbering
                        // convention of the environment into which it is ported.  Thus esp_err_t
                        // and WEAVE_ERROR are in fact the same type, and both ESP-IDF errors
                        // and Weave-specific errors can be stored in the same value without
                        // ambiguity.  For convenience, ESP_OK and WEAVE_NO_ERROR are mapped
                        // to the same value.

    // Initialize the ESP NVS layer.
    err = nvs_flash_init();
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "nvs_flash_init() failed: %s", ErrorStr(err));
        return;
    }

    // Initialize the LwIP core lock.  This must be done before the ESP
    // tcpip_adapter layer is initialized.
    err = PlatformMgrImpl().InitLwIPCoreLock();
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "PlatformMgr().InitLocks() failed: %s", ErrorStr(err));
        return;
    }

    // Initialize the ESP tcpip adapter.
    tcpip_adapter_init();

    // Arrange for the ESP event loop to deliver events into the Weave Device layer.
    err = esp_event_loop_init(PlatformManagerImpl::HandleESPSystemEvent, NULL);
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "esp_event_loop_init() failed: %s", ErrorStr(err));
        return;
    }

    // Initialize the ESP WiFi layer.
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "esp_event_loop_init() failed: %s", ErrorStr(err));
        return;
    }

    // Initialize the Weave stack.
    err = PlatformMgr().InitWeaveStack();
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "PlatformMgr().InitWeaveStack() failed: %s", ErrorStr(err));
        return;
    }

    // OpenThread requires more stacks, create another task to run it.
    ot_init();

    // Configure the Weave Connectivity Manager to automatically enable the WiFi AP interface
    // whenever the WiFi station interface has not be configured.
    ConnectivityMgr().SetWiFiAPMode(ConnectivityManager::kWiFiAPMode_OnDemand_NoStationProvision);

    // Register a function to receive events from the Weave device layer.  Note that calls to
    // this function will happen on the Weave event loop thread, not the app_main thread.
    PlatformMgr().AddEventHandler(DeviceEventHandler, 0);

#if CONFIG_ALIVE_INTERVAL
    // Start a Weave-based timer that will print an 'Alive' message on a periodic basis.  This
    // confirms that the Weave thread is alive and processing events.
    err = AliveTimer::Start(CONFIG_ALIVE_INTERVAL);
    if (err != WEAVE_NO_ERROR)
    {
        return;
    }
#endif // CONFIG_ALIVE_INTERVAL

    // Initialize the attention button.
    err = sAttentionButton.Init(ATTENTION_BUTTON_GPIO_NUM, 50);
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "Button.Init() failed: %s", ErrorStr(err));
        return;
    }

    ESP_LOGI(TAG, "Ready");

    // Start a task to run the Weave Device event loop.
    err = PlatformMgr().StartEventLoopTask();
    if (err != WEAVE_NO_ERROR)
    {
        ESP_LOGE(TAG, "PlatformMgr().StartEventLoopTask() failed: %s", ErrorStr(err));
        return;
    }

    // Repeatedly loop to drive the UI...
    while (true)
    {
        // Poll the attention button.  Whenever we detect a *release* of the button
        // demand start the WiFi AP interface and place the device in "user selected"
        // mode.
        //
        // While the device is in user selected mode, it will respond to Device
        // Identify Requests that have the UserSelectedMode flag set.  This makes it
        // easy for other mobile applications or devices to find it.
        //
        if (sAttentionButton.Poll() && !sAttentionButton.IsPressed())
        {
            PlatformMgr().LockWeaveStack();
            ConnectivityMgr().DemandStartWiFiAP();
            ConnectivityMgr().SetUserSelectedMode(true);
            PlatformMgr().UnlockWeaveStack();
        }

        // If the attention button has been pressed for more that the factory reset
        // press duration, initiate a factory reset of the device.
        if (sAttentionButton.IsPressed() &&
            sAttentionButton.GetStateDuration() > CONFIG_FACTORY_RESET_BUTTON_DURATION)
        {
            PlatformMgr().LockWeaveStack();
            ConfigurationMgr().InitiateFactoryReset();
            PlatformMgr().UnlockWeaveStack();
            return;
        }

        vTaskDelay(50 / portTICK_RATE_MS);
    }
}
