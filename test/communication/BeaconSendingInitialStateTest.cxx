/**
* Copyright 2018 Dynatrace LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "communication/BeaconSendingInitialState.h"
#include "communication/AbstractBeaconSendingState.h"

#include "../communication/MockBeaconSendingContext.h"
#include "../protocol/MockHTTPClient.h"
#include "../communication/CustomMatchers.h"

class BeaconSendingInitialStateTest : public testing::Test
{
public:

	BeaconSendingInitialStateTest()
		: mLogger(nullptr)
		, mTarget(nullptr)
		, mMockHTTPClient(nullptr)
	{
	}

	void SetUp()
	{
		mLogger = std::shared_ptr<openkit::ILogger>(new core::util::DefaultLogger(devNull, true));
		mTarget = std::shared_ptr<communication::AbstractBeaconSendingState>(new communication::BeaconSendingInitialState());
		std::shared_ptr<configuration::HTTPClientConfiguration> httpClientConfiguration = std::make_shared<configuration::HTTPClientConfiguration>(core::UTF8String(""),0, core::UTF8String(""));
		mMockHTTPClient = std::shared_ptr<testing::NiceMock<test::MockHTTPClient>>(new testing::NiceMock<test::MockHTTPClient>(httpClientConfiguration));
	}

	void TearDown()
	{
		mLogger = nullptr;
		mTarget = nullptr;
		mMockHTTPClient = nullptr;
	}

	std::ostringstream devNull;
	std::shared_ptr<openkit::ILogger> mLogger;
	std::shared_ptr<communication::AbstractBeaconSendingState> mTarget;
	std::shared_ptr<testing::NiceMock<test::MockHTTPClient>> mMockHTTPClient;
};

TEST_F(BeaconSendingInitialStateTest, initStateIsNotATerminalState)
{
	EXPECT_FALSE(mTarget->isTerminalState());
}

TEST_F(BeaconSendingInitialStateTest, getShutdownStateGivesABeaconSendingTerminalStateInstance)
{
	// when
	std::shared_ptr<AbstractBeaconSendingState> obtained = mTarget->getShutdownState();

	//then
	ASSERT_TRUE(obtained != nullptr);
	ASSERT_EQ(obtained->getStateType(), communication::AbstractBeaconSendingState::StateType::BEACON_SENDING_TERMINAL_STATE);
}

TEST_F(BeaconSendingInitialStateTest, getShutdownStateAlwaysCreatesANewInstance)
{
	// when calling getShutDownState twice
	std::shared_ptr<AbstractBeaconSendingState> obtainedOne = mTarget->getShutdownState();
	std::shared_ptr<AbstractBeaconSendingState> obtainedTwo = mTarget->getShutdownState();

	//then
	ASSERT_TRUE(obtainedOne != nullptr);
	ASSERT_TRUE(obtainedTwo != nullptr);
	EXPECT_NE(obtainedOne, obtainedTwo);
}

TEST_F(BeaconSendingInitialStateTest, executeSetsLastOpenSessionBeaconSendTime)
{
	testing::NiceMock<test::MockBeaconSendingContext> mockContext(mLogger);//NiceMock: ensure that required calls are there but do not object about other calls

	//given
	ON_CALL(mockContext, getHTTPClient())
		.WillByDefault(testing::Return(mMockHTTPClient));
	ON_CALL(*mMockHTTPClient, sendStatusRequestRawPtrProxy())
		.WillByDefault(testing::Return(new protocol::StatusResponse()));
	ON_CALL(mockContext, getCurrentTimestamp())
		.WillByDefault(testing::Return(123456789L));
	ON_CALL(mockContext, isShutdownRequested())
		.WillByDefault(testing::Return(true));
	
	// check
	EXPECT_CALL(mockContext, setLastOpenSessionBeaconSendTime(123456789L))
		.Times(::testing::Exactly(1));

	//when 
	mTarget->execute(mockContext);
}

TEST_F(BeaconSendingInitialStateTest, executeSetsLastStatusCheckTime)
{
	testing::NiceMock<test::MockBeaconSendingContext> mockContext(mLogger);//NiceMock: ensure that required calls are there but do not object about other calls

	//given
	ON_CALL(mockContext, getCurrentTimestamp())
		.WillByDefault(testing::Return(123456789L));
	ON_CALL(mockContext, isShutdownRequested())
		.WillByDefault(testing::Return(true));

	//check
	EXPECT_CALL(mockContext, setLastStatusCheckTime(123456789L))
		.Times(::testing::Exactly(1));

	//when
	mTarget->execute(mockContext);
}

TEST_F(BeaconSendingInitialStateTest, initIsTerminatedIfShutdownRequestedWithValidResponse)
{
	testing::NiceMock<test::MockBeaconSendingContext> mockContext(mLogger);//NiceMock: ensure that required calls are there but do not object about other calls

	//given
	ON_CALL(mockContext, isShutdownRequested())
		.WillByDefault(testing::Return(true));
	ON_CALL(mockContext, setNextState(testing::_))
		.WillByDefault(testing::WithArgs<0>(testing::Invoke(&mockContext, &test::MockBeaconSendingContext::RealSetNextState)));
	ON_CALL(mockContext, isInTerminalState())
		.WillByDefault(testing::Invoke(&mockContext, &test::MockBeaconSendingContext::RealIsInTerminalState));

	//check
	EXPECT_CALL(mockContext, setInitCompleted(false))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, setNextState(IsABeaconSendingTerminalState()))
		.Times(::testing::Exactly(1));

	//when
	mTarget->execute(mockContext);
	auto nextState = mockContext.RealGetNextState();
	EXPECT_TRUE(nextState != nullptr);
	EXPECT_TRUE(nextState->isTerminalState());
}



TEST_F(BeaconSendingInitialStateTest, sleepTimeIsDoubledBetweenStatusRequestRetries)
{
	auto mockContext = std::shared_ptr<testing::NiceMock<test::MockBeaconSendingContext>>(new testing::NiceMock<test::MockBeaconSendingContext>(mLogger)); //NiceMock: ensure that required calls are there but do not object about other calls

	uint32_t callCount = 0;
	//given
	ON_CALL(*mockContext, isShutdownRequested())
		.WillByDefault(testing::Invoke(
			[&callCount]() -> bool {
		return callCount++ >= 5;
	}
	));//should return true the 6th time

	ON_CALL(*mockContext, getHTTPClient())
		.WillByDefault(testing::Return(mMockHTTPClient));
	ON_CALL(*mMockHTTPClient, sendStatusRequestRawPtrProxy())
		.WillByDefault(testing::Return(nullptr));

	// check for 
	uint64_t initialSleep = communication::BeaconSendingInitialState::INITIAL_RETRY_SLEEP_TIME_MILLISECONDS.count();
	testing::InSequence s;
	EXPECT_CALL(*mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(*mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(*mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(*mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(*mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));

	// when executing the state
	mTarget->execute(*mockContext);
}

TEST_F(BeaconSendingInitialStateTest, initialStatusRequestGivesUpWhenShutdownRequestIsSetDuringExecution)
{
	testing::NiceMock<test::MockBeaconSendingContext> mockContext(mLogger);//NiceMock: ensure that required calls are there but do not object about other calls
	

	// given
	ON_CALL(mockContext, getHTTPClient())
		.WillByDefault(testing::Return(mMockHTTPClient));
	ON_CALL(*mMockHTTPClient, sendStatusRequestRawPtrProxy())
		.WillByDefault(testing::Return(nullptr));
	ON_CALL(mockContext, setNextState(testing::_))
		.WillByDefault(testing::WithArgs<0>(testing::Invoke(&mockContext, &test::MockBeaconSendingContext::RealSetNextState)));

	uint32_t callCount = 0;
	//given
	ON_CALL(mockContext, isShutdownRequested())
		.WillByDefault(testing::Invoke(
			[&callCount]() -> bool {
		return callCount++ >= 2;
	}
	));//should return true the 3rd time

	// then
	EXPECT_CALL(mockContext, setInitCompleted(false))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, setNextState(IsABeaconSendingTerminalState()))
		.Times(::testing::Exactly(1));
							
	// verify that the requests where sent N times - defined as constants in the state itself
	EXPECT_CALL(mockContext, getHTTPClient())
		.Times(::testing::Exactly(3));

	// verify sleeps between each retry
	EXPECT_CALL(mockContext, sleep(testing::_))
		.Times(::testing::Exactly(2));

	// when executing the state
	mTarget->execute(mockContext);
	auto nextState = mockContext.RealGetNextState();
	ASSERT_TRUE(nextState != nullptr);
	ASSERT_TRUE(nextState->isTerminalState());
}

TEST_F(BeaconSendingInitialStateTest, aSuccessfulStatusResponsePerformsStateTransitionToTimeSyncState)
{
	testing::NiceMock<test::MockBeaconSendingContext> mockContext(mLogger);//NiceMock: ensure that required calls are there but do not object about other calls

	// given
	ON_CALL(mockContext, getHTTPClient())
		.WillByDefault(testing::Return(mMockHTTPClient));
	ON_CALL(*mMockHTTPClient, sendStatusRequestRawPtrProxy())
		.WillByDefault(testing::Return(new protocol::StatusResponse()));

	// verify state transition
	EXPECT_CALL(mockContext, setNextState(IsABeaconSendingTimeSyncState()))
		.Times(::testing::Exactly(1));

	// when
	mTarget->execute(mockContext);
}

TEST_F(BeaconSendingInitialStateTest, reinitializeSleepsBeforeSendingStatusRequestsAgain)
{
	testing::NiceMock<test::MockBeaconSendingContext> mockContext(mLogger);//NiceMock: ensure that required calls are there but do not object about other calls

	uint32_t callCount = 0;
	//given
	ON_CALL(mockContext, isShutdownRequested())
		.WillByDefault(testing::Invoke(
			[&callCount]() -> bool {
				return callCount++ >= 41; 
				}
		));//should return true the 42th time

	ON_CALL(mockContext, getHTTPClient())
		.WillByDefault(testing::Return(mMockHTTPClient));
	ON_CALL(*mMockHTTPClient, sendStatusRequestRawPtrProxy())
		.WillByDefault(testing::Return(nullptr));

	testing::InSequence s;

	uint64_t initialSleep = communication::BeaconSendingInitialState::INITIAL_RETRY_SLEEP_TIME_MILLISECONDS.count();
	
	// then
	// from first round
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));
	// delay between first and second attempt
	EXPECT_CALL(mockContext, sleep(communication::BeaconSendingInitialState::REINIT_DELAY_MILLISECONDS[0].count()))
		.Times(::testing::Exactly(1));
	// and again the sequence
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));
	// delay between second and third attempt
	EXPECT_CALL(mockContext, sleep(communication::BeaconSendingInitialState::REINIT_DELAY_MILLISECONDS[1].count()))
		.Times(::testing::Exactly(1));
	// and again the sequence
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));
	// delay between third and fourth attempt
	EXPECT_CALL(mockContext, sleep(communication::BeaconSendingInitialState::REINIT_DELAY_MILLISECONDS[2].count()))
		.Times(::testing::Exactly(1));
	// and again the sequence
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));
	// delay between fourth and fifth attempt
	EXPECT_CALL(mockContext, sleep(communication::BeaconSendingInitialState::REINIT_DELAY_MILLISECONDS[3].count()))
		.Times(::testing::Exactly(1));
	// and again the sequence
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));
	// delay between fifth and sixth attempt
	EXPECT_CALL(mockContext, sleep(communication::BeaconSendingInitialState::REINIT_DELAY_MILLISECONDS[4].count()))
		.Times(::testing::Exactly(1));
	// and again the sequence
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));
	// delay between sixth and seventh attempt
	EXPECT_CALL(mockContext, sleep(communication::BeaconSendingInitialState::REINIT_DELAY_MILLISECONDS[4].count()))
		.Times(::testing::Exactly(1));
	// and again the sequence
	EXPECT_CALL(mockContext, sleep(initialSleep))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 2))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 4))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 8))
		.Times(::testing::Exactly(1));
	EXPECT_CALL(mockContext, sleep(initialSleep * 16))
		.Times(::testing::Exactly(1));

	// when executing the state multiple times (7 times)
	mTarget->execute(mockContext);
}
