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
#include "OmapiTransport.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include <android-base/logging.h>
#include <android-base/properties.h>


uint8_t IAR_APPLET_AID[] = {0xA0, 0x00, 0x00, 0x08, 0x44, 0x53, 0xF1, 0x28, 0x70, 0x03, 0x01, 0x00};
uint8_t AID_SIZE = 12;

std::string const ESE_READER_PREFIX = "eSE";
constexpr const char omapiServiceName[] =
        "android.se.omapi.ISecureElementService/default";

class SEListener : public ::aidl::android::se::omapi::BnSecureElementListener {};

#ifdef ENABLE_SESSION_TIMEOUT
Timer sessionTimer;
#endif

bool OmapiTransport::initialize() {

    LOG(DEBUG) << "Initialize the secure element connection";

    // Get OMAPI vendor stable service handler
    ::ndk::SpAIBinder ks2Binder(AServiceManager_checkService(omapiServiceName));
    omapiSeService = aidl::android::se::omapi::ISecureElementService::fromBinder(ks2Binder);

    if (omapiSeService == nullptr) {
        LOG(ERROR) << "Failed to start omapiSeService null";
        return false;
    }

    // reset readers, clear readers if already existing
    if (mVSReaders.size() > 0) {
        closeConnection();
    }

    std::vector<std::string> readers = {};
    // Get available readers
    auto status = omapiSeService->getReaders(&readers);
    if (!status.isOk()) {
        LOG(ERROR) << "getReaders failed to get available readers: " << status.getMessage();
        return false;
    }

    // Get SE readers handlers
    for (auto readerName : readers) {
        std::shared_ptr<::aidl::android::se::omapi::ISecureElementReader> reader;
        status = omapiSeService->getReader(readerName, &reader);
        if (!status.isOk()) {
            LOG(ERROR) << "getReader for " << readerName.c_str()
                       << " Failed: " << status.getMessage();
            return false;
        }
        mVSReaders[readerName] = reader;
    }

    // Find eSE reader, as of now assumption is only eSE available on device
    LOG(DEBUG) << "Finding eSE reader";
    eSEReader = nullptr;
    if (mVSReaders.size() > 0) {
        for (const auto& [name, reader] : mVSReaders) {
            if (name.find(ESE_READER_PREFIX, 0) != std::string::npos) {
                LOG(DEBUG) << "eSE reader found: " << name;
                eSEReader = reader;
                break;
            }
        }
    }

    if (eSEReader == nullptr) {
        LOG(ERROR) << "secure element reader " << ESE_READER_PREFIX << " not found";
        return false;
    }

    bool isSecureElementPresent = false;
    auto res = eSEReader->isSecureElementPresent(&isSecureElementPresent);
    if (!res.isOk()) {
        eSEReader = nullptr;
        LOG(ERROR) << "isSecureElementPresent error: " << res.getMessage();
        return false;
    }
    if (!isSecureElementPresent) {
        LOG(ERROR) << "secure element not found";
        eSEReader = nullptr;
        return false;
    }
    
    channel = nullptr;
    session = nullptr;

    return true;
}

bool OmapiTransport::internalTransmitApdu(
    std::shared_ptr<aidl::android::se::omapi::ISecureElementReader> reader,
    std::vector<uint8_t> apdu, std::vector<uint8_t>& transmitResponse) {

    LOG(DEBUG) << "internalTransmitApdu: trasmitting data to secure element";

#ifdef ENBALE_SESSION_TIMEOUT
    // Stop the timer
    LOG(DEBUG) << "Stop timeout if any.";
    sessionTimer.stop();
#endif

    if (reader == nullptr) {
        LOG(ERROR) << "eSE reader is null";
        return false;
    }

    bool result = true;
    auto res = ndk::ScopedAStatus::ok();
    if(session != nullptr) {
        res = session->isClosed(&result);
        if (!res.isOk()) {
            LOG(ERROR) << "isClosed error: " << res.getMessage();
            return false;
        }
    }
    if(result) {
        res = reader->openSession(&session);
        if (!res.isOk()) {
            LOG(ERROR) << "openSession error: " << res.getMessage();
            return false;
        }
        if (session == nullptr) {
            LOG(ERROR) << "Could not open session null";
            return false;
        }
    }

    result = true;
    if(channel != nullptr) {
        res = channel->isClosed(&result);
        if (!res.isOk()) {
            LOG(ERROR) << "isClosed error: " << res.getMessage();
            return false;
        }
    }

    int size = AID_SIZE;
    std::vector<uint8_t> aid(IAR_APPLET_AID, IAR_APPLET_AID + size);
    if (result) {
        auto mSEListener = ndk::SharedRefBase::make<SEListener>();
        res = session->openLogicalChannel(aid, 0x00, mSEListener, &channel);
        if (!res.isOk()) {
            LOG(ERROR) << "openLogicalChannel error: " << res.getMessage();
            return false;
        }
        if (channel == nullptr) {
            LOG(ERROR) << "Could not open channel null";
            return false;
        }
    }

    std::vector<uint8_t> selectResponse = {};
    res = channel->getSelectResponse(&selectResponse);
    if (!res.isOk()) {
        LOG(ERROR) << "getSelectResponse error: " << res.getMessage();
        return false;
    }

    if ((selectResponse.size() < 2)
        || ((selectResponse[selectResponse.size() -1] & 0xFF) != 0x00)
        || ((selectResponse[selectResponse.size() -2] & 0xFF) != 0x90))
    {
        LOG(ERROR) << "Failed to select the Applet.";
        return false;
    }

    res = channel->transmit(apdu, &transmitResponse);

    LOG(INFO) << "STATUS OF TRNSMIT: " << res.getExceptionCode()
              << " Message: " << res.getMessage();
    if (!res.isOk()) {
        LOG(ERROR) << "transmit error: " << res.getMessage();
        return false;
    }

#ifdef ENABLE_SESSION_TIMEOUT
    LOG(DEBUG) << "Start timeout before closing channels ";
    if ( apdu.size() > 2 && apdu.at(1) == 0x30 ) {
        sessionTimer.count++;
    } else if ( apdu.size() > 2 && apdu.at(1) == 0x32 ) {
        sessionTimer.count--;
    }
    if ( sessionTimer.count > 0 ) {
        sessionTimer.start(SESSION_TIMEOUT_300S, this);
    } else {
        sessionTimer.start(SESSION_TIMEOUT_20S, this);
    }
#endif

    return true;
}

bool OmapiTransport::openConnection() {

    // if already conection setup done, no need to initialise it again.
    if (isConnected()) {
        return false;
    }
    return initialize();
}

bool OmapiTransport::sendData(const vector<uint8_t>& inData, vector<uint8_t>& output) {

    if (!isConnected()) {
        // Try to initialize connection to eSE
        LOG(INFO) << "Failed to send data, try to initialize connection SE connection";
        auto res = initialize();
        if (!res) {
            LOG(ERROR) << "Failed to send data, initialization not completed";
            closeConnection();
            return res;
        }
    }

    if (eSEReader != nullptr) {
        LOG(DEBUG) << "Sending apdu data to secure element: " << ESE_READER_PREFIX;
        if(internalTransmitApdu(eSEReader, inData, output)) {
            closeConnection();
            return true;
        } else {
            closeConnection();
            return false;
        }
    } else {
        LOG(ERROR) << "secure element reader " << ESE_READER_PREFIX << " not found";
        return false;
    }
}

bool OmapiTransport::closeConnection() {
    LOG(DEBUG) << "Closing all connections";
    if (channel != nullptr) channel->close();
    if (session != nullptr) session->close();
    if (omapiSeService != nullptr) {
        if (mVSReaders.size() > 0) {
            for (const auto& [name, reader] : mVSReaders) {
                reader->closeSessions();
            }
            mVSReaders.clear();
        }
    }
    omapiSeService = nullptr;
    eSEReader = nullptr;
    return true;
}

bool OmapiTransport::isConnected() {
    // Check already initialization completed or not
    if (omapiSeService != nullptr && eSEReader != nullptr) {
        LOG(DEBUG) << "Connection initialization already completed";
        return true;
    }

    LOG(DEBUG) << "Connection initialization not completed";
    return false;
}
