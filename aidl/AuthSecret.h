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

#pragma once

#include <aidl/android/hardware/authsecret/BnAuthSecret.h>
#include "OmapiTransport.h"

namespace aidl {
namespace android {
namespace hardware {
namespace authsecret {

struct AuthSecret : public BnAuthSecret {
    //AuthSecret() = default;
    AuthSecret() {
        mTransport = std::unique_ptr<OmapiTransport>(new OmapiTransport());
    }
    
    virtual ~AuthSecret() { mTransport->closeConnection(); }
    
    bool constructApduMessage(uint8_t ins, const std::vector<uint8_t>& inputData,
                                           std::vector<uint8_t>& apduOut);

    inline uint16_t getApduStatus(std::vector<uint8_t>& inputData) {
        // Last two bytes are the status SW0SW1
        uint8_t SW0 = inputData.at(inputData.size() - 2);
        uint8_t SW1 = inputData.at(inputData.size() - 1);
        return (SW0 << 8 | SW1);
    }

    bool clearFlag();
    // Methods from ::android::hardware::authsecret::IAuthSecret follow.
    ::ndk::ScopedAStatus setPrimaryUserCredential(const std::vector<uint8_t>& in_secret) override;
    
    private:
    /**
     * Holds the instance of either OmapiTransport class or SocketTransport class.
     */
    std::unique_ptr<ITransport> mTransport;

};

} // namespace authsecret
} // namespace hardware
} // namespace android
} // aidl
