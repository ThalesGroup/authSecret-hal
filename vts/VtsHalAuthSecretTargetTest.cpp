/*
 * Copyright (C) 2020 The Android Open Source Project
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
#include <aidl/Gtest.h>
#include <aidl/Vintf.h>

#include <aidl/android/hardware/authsecret/IAuthSecret.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include "OmapiTransport.h"

using ::aidl::android::hardware::authsecret::IAuthSecret;

using ::ndk::SpAIBinder;

/**
 * There is no expected behaviour that can be tested so these tests check the
 * HAL doesn't crash with different execution orders.
 */
 
static std::unique_ptr<ITransport> mTransport;

class AuthSecretAidlTest : public testing::TestWithParam<std::string> {
  public:
    
    virtual void SetUp() override {
        authsecret = IAuthSecret::fromBinder(
            SpAIBinder(AServiceManager_waitForService(GetParam().c_str())));
        ASSERT_NE(authsecret, nullptr);

        /*// Notify LSS to generate PIN code '1234' and corresponding secret.
        (void)system("cmd lock_settings set-pin 1234");

        // All tests must enroll the correct secret first as this cannot be changed
        // without a factory reset and the order of tests could change.
        authsecret->setPrimaryUserCredential(CORRECT_SECRET);*/

        bool ret = false;
        mTransport = std::unique_ptr<OmapiTransport>(new OmapiTransport());
        if (mTransport == nullptr) return;
        ret = mTransport->openConnection();
        if (!ret) {
            return;
        }

        ret = clear_auth_secret();
        if (!ret) {
            return;
        }
        ret = set_auth_secret();
        if (!ret) {
            return;
        }
    }

    static bool set_auth_secret() {
        std::vector<uint8_t> response;
        std::vector<uint8_t> SET_SECRET_APDU{0x80, 0x54, 0x00, 0x00, 0x10, 61, 93, 124, 240, 5, 0, 7, 201, 9, 129, 11, 12, 0, 14, 0, 16};
        if(mTransport == nullptr) return false;
        return mTransport->sendData(SET_SECRET_APDU, response);
    }

    static bool clear_auth_secret() {
        std::vector<uint8_t> response;
        std::vector<uint8_t> CLEAR_SECRET_APDU{0x80, 0x56, 0x00, 0x00, 0x00};
        if(mTransport == nullptr) return false;
        return mTransport->sendData(CLEAR_SECRET_APDU, response);
    }

    static void TearDownTestSuite() {
        /*// clean up PIN code after testing
        (void)system("cmd lock_settings clear --old 1234");*/

        clear_auth_secret();

        mTransport->closeConnection();
    }

    std::shared_ptr<IAuthSecret> authsecret;
    std::vector<uint8_t> CORRECT_SECRET{61, 93, 124, 240, 5, 0, 7, 201, 9, 129, 11, 12, 0, 14, 0, 16};
    std::vector<uint8_t> WRONG_SECRET{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
};

/* Provision the primary user with a secret. */
TEST_P(AuthSecretAidlTest, provisionPrimaryUserCredential) {
    // Secret provisioned by SetUp()
}

/* Provision the primary user with a secret and pass the secret again. */
TEST_P(AuthSecretAidlTest, provisionPrimaryUserCredentialAndPassAgain) {
    // Secret provisioned by SetUp()
    authsecret->setPrimaryUserCredential(CORRECT_SECRET);
}

/* Provision the primary user with a secret and pass the secret again repeatedly. */
TEST_P(AuthSecretAidlTest, provisionPrimaryUserCredentialAndPassAgainMultipleTimes) {
    // Secret provisioned by SetUp()
    constexpr int N = 5;
    for (int i = 0; i < N; ++i) {
        authsecret->setPrimaryUserCredential(CORRECT_SECRET);
    }
}

/* Provision the primary user with a secret and then pass the wrong secret. This
 * should never happen and is an framework bug if it does. As the secret is
 * wrong, the HAL implementation may not be able to function correctly but it
 * should fail gracefully. */
TEST_P(AuthSecretAidlTest, provisionPrimaryUserCredentialAndWrongSecret) {
    // Secret provisioned by SetUp()
    authsecret->setPrimaryUserCredential(WRONG_SECRET);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AuthSecretAidlTest);
INSTANTIATE_TEST_SUITE_P(
        PerInstance, AuthSecretAidlTest,
        testing::ValuesIn(android::getAidlHalInstanceNames(IAuthSecret::descriptor)),
        android::PrintInstanceNameToString);

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();
    return RUN_ALL_TESTS();
}
