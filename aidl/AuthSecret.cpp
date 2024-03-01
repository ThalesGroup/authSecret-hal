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
#include <log/log.h>

#define APDU_CLS 0x80
#define INS_SET_AUTH_SECRET 0x54
#define APDU_P1 0x00
#define APDU_P2 0x00
#define APDU_RESP_STATUS_OK 0x9000

namespace aidl {
namespace android {
namespace hardware {
namespace authsecret {


void
dump_bytes(const char *pf, char sep, const uint8_t *p, int n, FILE *out)
{
    const uint8_t *s = p;
    char *msg;
    int len = 0;
    int input_len = n;


    msg = (char*) malloc ( (pf ? strlen(pf) : 0) + input_len * 3 + 1);
    if(!msg) {
        errno = ENOMEM;
        return;
    }

    if (pf) {
        len += sprintf(msg , "%s" , pf);
    }
    while (input_len--) {
        len += sprintf(msg + len, "%02X" , *s++);
        if (input_len && sep) {
            len += sprintf(msg + len, ":");
        }
    }
    sprintf(msg + len, "\n");
    ALOGD("SecureElement:%s ==> size = %d data = %s", __func__, n, msg);

    if(msg) free(msg);
}

bool AuthSecret::constructApduMessage(uint8_t ins, const std::vector<uint8_t>& inputData, std::vector<uint8_t>& apduOut) {

    apduOut.push_back(static_cast<uint8_t>(APDU_CLS));  // CLS
    apduOut.push_back(static_cast<uint8_t>(ins));       // INS
    apduOut.push_back(static_cast<uint8_t>(APDU_P1));   // P1
    apduOut.push_back(static_cast<uint8_t>(APDU_P2));   // P2

    if (USHRT_MAX >= inputData.size()) {
        // Send extended length APDU always as response size is not known to HAL.
        // Case 1: Lc > 0  CLS | INS | P1 | P2 | 00 | 2 bytes of Lc | CommandData | 2 bytes of Le
        // all set to 00. Case 2: Lc = 0  CLS | INS | P1 | P2 | 3 bytes of Le all set to 00.
        // Extended length 3 bytes, starts with 0x00
        apduOut.push_back(static_cast<uint8_t>(0x00));
        if (inputData.size() > 0) {
            apduOut.push_back(static_cast<uint8_t>(inputData.size() >> 8));
            apduOut.push_back(static_cast<uint8_t>(inputData.size() & 0xFF));
            // Data
            apduOut.insert(apduOut.end(), inputData.begin(), inputData.end());
        }
        // Expected length of output.
        // Accepting complete length of output every time.
        apduOut.push_back(static_cast<uint8_t>(0x00));
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

    buffer = (uint8_t*)malloc(buffer_len * sizeof(uint8_t));

    memcpy(buffer, in_secret.data(), in_secret.size());
    dump_bytes("in_secret: ", ':', buffer, buffer_len, stdout);
    
    mTransport->openConnection();
    
    std::vector<uint8_t> apdu;
    std::vector<uint8_t> response;

    ret = constructApduMessage(INS_SET_AUTH_SECRET, in_secret, apdu);

    if (!ret) {
        return ::ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Error in constructApduMessage.");
    }

    ret = mTransport->sendData(apdu, response);
    if (!ret) {
        ALOGE("Error in sending data in sendData. %d", static_cast<int>(ret));
        return ::ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_TRANSACTION_FAILED, "Error in sending data in sendData.");
    }

    // Response size should be greater than 2. Cbor output data followed by two bytes of APDU
    // status.
    if (response.size() <= 2){
        ALOGE("Response of the sendData is wrong: response size = %d",response.size());
        return ::ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_TRANSACTION_FAILED, "Response of the sendData is wrong. Less than 2 bytes.");
    } else if (getApduStatus(response) != APDU_RESP_STATUS_OK) {
        ALOGE("Response of the sendData is wrong: apdu status = %ld",getApduStatus(response));
        return ::ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_TRANSACTION_FAILED, "Response of the sendData is wrong.");
    }
    
    return ::ndk::ScopedAStatus::ok();
}

} // namespace authsecret
} // namespace hardware
} // namespace android
} // aidl
