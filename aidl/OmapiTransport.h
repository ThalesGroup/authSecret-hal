/*
 **
 * Copyright (C) 2024 Thales
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */
 #pragma once

#include <map>
#include <memory>
#include <vector>


#include <aidl/android/se/omapi/BnSecureElementListener.h>
#include <aidl/android/se/omapi/ISecureElementChannel.h>
#include <aidl/android/se/omapi/ISecureElementListener.h>
#include <aidl/android/se/omapi/ISecureElementReader.h>
#include <aidl/android/se/omapi/ISecureElementService.h>
#include <aidl/android/se/omapi/ISecureElementSession.h>

#include <android/binder_manager.h>

#include "ITransport.h"

// Session timeout
#define SESSION_TIMEOUT_20S (20000)  // 20 s
#define SESSION_TIMEOUT_300S (300000) // 300s

using std::vector;

/**
 * OmapiTransport is derived from ITransport. This class gets the OMAPI service binder instance and
 * uses IPC to communicate with OMAPI service. OMAPI inturn communicates with hardware via
 * ISecureElement.
 */
class OmapiTransport : public ITransport {

  public:
    OmapiTransport() : ITransport() {}
    /**
     * Gets the binder instance of ISEService, gets te reader corresponding to secure element,
     * establishes a session and opens a basic channel.
     */
    bool openConnection() override;
    /**
     * Transmists the data over the opened basic channel and receives the data back.
     */
    bool sendData(const vector<uint8_t>& inData, vector<uint8_t>& output) override;

    /**
     * Closes the connection.
     */
    bool closeConnection() override;
    /**
     * Returns the state of the connection status. Returns true if the connection is active, false
     * if connection is broken.
     */
    bool isConnected() override;


  private:
    std::shared_ptr<aidl::android::se::omapi::ISecureElementService> omapiSeService = nullptr;
    std::shared_ptr<aidl::android::se::omapi::ISecureElementReader> eSEReader = nullptr;
    std::shared_ptr<aidl::android::se::omapi::ISecureElementSession> session = nullptr;
    std::shared_ptr<aidl::android::se::omapi::ISecureElementChannel> channel = nullptr;
    std::map<std::string, std::shared_ptr<aidl::android::se::omapi::ISecureElementReader>>
        mVSReaders = {};
    bool initialize();
    bool
    internalTransmitApdu(std::shared_ptr<aidl::android::se::omapi::ISecureElementReader> reader,
                         std::vector<uint8_t> apdu, std::vector<uint8_t>& transmitResponse);
};
