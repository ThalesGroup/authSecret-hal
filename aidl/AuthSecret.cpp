/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Methods from ::android::hardware::authsecret::IAuthSecret follow.
::ndk::ScopedAStatus AuthSecret::setPrimaryUserCredential(const std::vector<uint8_t>& in_secret) {
    (void)in_secret;
    uint8_t *buffer;
    int buffer_len = in_secret.size();

    buffer = (uint8_t*)malloc(buffer_len * sizeof(uint8_t));

    memcpy(buffer, in_secret.data(), in_secret.size());
    dump_bytes("in_secret: ", ':', buffer, buffer_len, stdout);
    return ::ndk::ScopedAStatus::ok();
}

} // namespace authsecret
} // namespace hardware
} // namespace android
} // aidl
