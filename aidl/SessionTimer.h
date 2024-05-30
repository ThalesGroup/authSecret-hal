/******************************************************************************
 **
 ** Copyright ©2023-2024 THALES. All rights Reserved.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 ** http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 **
 ** 
 **
 *********************************************************************************/
#pragma once
#ifdef ENABLE_CLEAR_FLAG_TIMEOUT
#include <iostream>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <csignal>
#include <unistd.h>
#include <atomic>
#include "AuthSecret.h"

#define SESSION_TIMEOUT_5M (300000)  // 5 minutes

namespace aidl {
namespace android {
namespace hardware {
namespace authsecret {

class Timer {
public:
    int count = 0;
    Timer() : is_running(false) {}

    ~Timer() {
        stop();
    }

    // Start the timer with the specified timeout and call closeChannel if timeout is reached
    void start(int timeout_ms, void* ptr) {
        if (!is_running) {
            is_running = true;
            authSecret_ptr = ptr;
            
            if (std::signal(SIGALRM, &timerCallback) == SIG_ERR) {
                LOG(ERROR) << "Error setting up signal handler for SIGALRM. " << std::endl;
                return;
            }
            
            if (alarm(timeout_ms / 1000) != 0) {
                LOG(ERROR) << "Error setting the alarm. " << std::endl;
                return;
            }
        }
    }

    // Stop the timer
    void stop() {
        if (is_running) {
            is_running = false;
            alarm(0);
        }
    }

private:
    std::atomic<bool> is_running;
    static void* authSecret_ptr;

    // Static callback function required by timer_create
    static void timerCallback(int signal) {
        LOG(DEBUG) << "signal: " << signal;
        aidl::android::hardware::authsecret::AuthSecret *auth_secret = (aidl::android::hardware::authsecret::AuthSecret*)authSecret_ptr;
        if (auth_secret != nullptr) {
            auth_secret->clearFlag();
        }
    }
};

} // namespace authsecret
} // namespace hardware
} // namespace android
} // aidl
#endif
