/**
 * Copyright 2018-2021 Dynatrace LLC
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

#ifndef _TEST_CORE_UTIL_MOCKIINTERRUPTIBLETHREADSUSPENDER_H
#define _TEST_CORE_UTIL_MOCKIINTERRUPTIBLETHREADSUSPENDER_H

#include "core/util/IInterruptibleThreadSuspender.h"

#include <cstdint>

#include "gmock/gmock.h"

namespace test
{
	class MockIInterruptibleThreadSuspender
		: public core::util::IInterruptibleThreadSuspender
	{
	public:

		~MockIInterruptibleThreadSuspender() override = default;

		static std::shared_ptr<testing::NiceMock<MockIInterruptibleThreadSuspender>> createNice()
		{
			return std::make_shared<testing::NiceMock<MockIInterruptibleThreadSuspender>>();
		}

		static std::shared_ptr<testing::StrictMock<MockIInterruptibleThreadSuspender>> createStrict()
		{
			return std::make_shared<testing::StrictMock<MockIInterruptibleThreadSuspender>>();
		}

		MOCK_METHOD(void, sleep, (int64_t), (override));

		MOCK_METHOD(void, wakeup, (), (override));
	};
}

#endif
