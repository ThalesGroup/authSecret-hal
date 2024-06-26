/*
 * Copyright (C) 2024 Thales
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AuthSecret.h"
#include <android-base/result.h>
#include <android-base/stringprintf.h>
#include <android/binder_ibinder.h>
#include <log/log.h>
#include <android-base/properties.h>
#include <inttypes.h>

#define APDU_CLS 0x80
#define INS_SET_AUTH_SECRET 0x54
#define APDU_P1 0x00
#define APDU_P2 0x00
#define APDU_RESP_STATUS_OK 0x9000

namespace aidl {
namespace android {
namespace hardware {
namespace authsecret {

bool AuthSecret::constructApduMessage(uint8_t ins, const std::vector<uint8_t>& inputData, std::vector<uint8_t>& apduOut) {

    apduOut.push_back(static_cast<uint8_t>(APDU_CLS));  // CLS
    apduOut.push_back(static_cast<uint8_t>(ins));       // INS
    apduOut.push_back(static_cast<uint8_t>(APDU_P1));   // P1
    apduOut.push_back(static_cast<uint8_t>(APDU_P2));   // P2

    if (USHRT_MAX >= inputData.size()) {
        if (inputData.size() > 0) {
            apduOut.push_back(static_cast<uint8_t>(inputData.size() & 0xFF));
            // Data
            apduOut.insert(apduOut.end(), inputData.begin(), inputData.end());
        }
        // Expected length of output.
        // Accepting complete length of output every time.
        apduOut.push_back(static_cast<uint8_t>(0x00));
    } else {
        ALOGE( "Error in constructApduMessage.");
        return false;
    }
    return true;  // success
}

// Methods from ::android::hardware::authsecret::IAuthSecret follow.
::ndk::ScopedAStatus AuthSecret::setPrimaryUserCredential(const std::vector<uint8_t>& in_secret) {
    (void)in_secret;
    uint8_t *buffer;
    int buffer_len = in_secret.size();
    bool ret = false;
    char logmsg[500];

    ALOGD(">>> %s", __func__);
    buffer = (uint8_t*)malloc(buffer_len * sizeof(uint8_t));

    memcpy(buffer, in_secret.data(), in_secret.size());

    ret = mTransport->openConnection();
    if (!ret) {
        ALOGE("Error openning connection ret = %d", static_cast<int>(ret));
        return ndk::ScopedAStatus::ok();
    }

    std::vector<uint8_t> apdu;
    std::vector<uint8_t> response;

    ret = constructApduMessage(INS_SET_AUTH_SECRET, in_secret, apdu);
    if (!ret) {
        ALOGE("Error during APDU construction ret = %d", static_cast<int>(ret));
        return ndk::ScopedAStatus::ok();
    }

    ALOGD("mTransport->sendData(...)", __func__);
    ret = mTransport->sendData(apdu, response);
    if (!ret) {
        ALOGE("Error in sending data in sendData. ret = %d", static_cast<int>(ret));
        return ndk::ScopedAStatus::ok();
    }

    // Response size should be greater than 2. Cbor output data followed by two bytes of APDU
    // status.
    if (response.size() < 2){
        ALOGE("Response of the sendData is wrong: response size = %d",response.size());
        return ndk::ScopedAStatus::ok();
    } else if (getApduStatus(response) != APDU_RESP_STATUS_OK) {
        ALOGE("Response of the sendData is wrong: apdu status = %ld",getApduStatus(response));
        return ndk::ScopedAStatus::ok();
    }

    return ndk::ScopedAStatus::ok();
}

} // namespace authsecret
} // namespace hardware
} // namespace android
} // aidl
