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

#include <stdint.h>
#include <iostream>

#include "core/OpenKit.h"
#include "api/ISession.h"

#include "core/BeaconSender.h"
#include "core/util/CommandLineArguments.h"
#include "core/util/DefaultLogger.h"

#include "providers/DefaultSessionIDProvider.h"

#include "configuration/HTTPClientConfiguration.h"
#include "configuration/OpenKitType.h"
#include "configuration/Device.h"
#include "protocol/ssl/SSLStrictTrustManager.h"
#include "protocol/Beacon.h"
#include "core/Session.h"
#include "core/RootAction.h"
#include "caching/BeaconCache.h"

using namespace core;
using namespace communication;
using namespace providers;
using namespace configuration;

constexpr char APPLICATION_VERSION[] = "1.2.3";

void parseCommandLine(uint32_t argc, char** argv, UTF8String& beaconURL, uint32_t& serverID, UTF8String& applicationID)
{
	core::util::CommandLineArguments commandLine;
	commandLine.parse(argc, argv);

	if (commandLine.isValidConfiguration())
	{
		beaconURL = commandLine.getBeaconURL();
		serverID = commandLine.getServerID();
		applicationID = commandLine.getApplicationID();
	}
	else
	{
		std::cerr << "Error: Configuration given on command line is not valid." << std::endl;
		commandLine.printHelp();
		std::cerr << "Exiting." << std::endl;
		exit(-1);
	}

}

int32_t main(int32_t argc, char** argv)
{
	UTF8String beaconURL;
	uint32_t serverID = 0;
	UTF8String applicationID;

	parseCommandLine(argc, argv, beaconURL, serverID, applicationID);

	auto logger = std::shared_ptr<api::ILogger>(new core::util::DefaultLogger(true));
	std::shared_ptr<protocol::ISSLTrustManager> trustManager = std::make_shared<protocol::SSLStrictTrustManager>();

	std::shared_ptr<ISessionIDProvider> sessionIDProvider = std::shared_ptr<ISessionIDProvider>(new DefaultSessionIDProvider());


	std::shared_ptr<configuration::Device> device = std::shared_ptr<configuration::Device>(new configuration::Device(core::UTF8String("ACME OS"), core::UTF8String("Dynatrace"), core::UTF8String("Model E")));
	std::shared_ptr<configuration::BeaconCacheConfiguration> beaconCacheConfiguration = std::make_shared<configuration::BeaconCacheConfiguration>(
		configuration::BeaconCacheConfiguration::DEFAULT_MAX_RECORD_AGE_IN_MILLIS.count(),
		configuration::BeaconCacheConfiguration::DEFAULT_UPPER_MEMORY_BOUNDARY_IN_BYTES,
		configuration::BeaconCacheConfiguration::DEFAULT_LOWER_MEMORY_BOUNDARY_IN_BYTES
		);

	std::shared_ptr<Configuration> configuration = std::shared_ptr<Configuration>(new Configuration(device, configuration::OpenKitType::DYNATRACE,
																									core::UTF8String("openkit-sample"), APPLICATION_VERSION, applicationID, serverID, beaconURL,
																									sessionIDProvider, trustManager, beaconCacheConfiguration ));

	OpenKit openKit(logger, configuration);
	openKit.initialize();
	openKit.waitForInitCompletion(20000);

	if (openKit.isInitialized())
	{
		std::shared_ptr<api::ISession> sampleSession = openKit.createSession("172.16.23.30");
		sampleSession->identifyUser("test user");

		auto rootAction1 = sampleSession->enterAction("root action");
		auto childAction1 = rootAction1->enterAction("child action");

		rootAction1->reportValue("the answer", 42);

		childAction1->reportValue("some string", "1337.3.1415");

		auto webRequest = childAction1->traceWebRequest("http://www.stackoverflow.com/");
		webRequest->start();

		std::this_thread::sleep_for(std::chrono::milliseconds(144));

		webRequest->setResponseCode(200);
		webRequest->setBytesSent(123);
		webRequest->setBytesReceived(45);
		webRequest->stop();

		childAction1->leaveAction();
		rootAction1->leaveAction();

		sampleSession->end();
	}

	openKit.shutdown();

	return 0;
}