// Scenario-based test harness for REQ_TestClient
// This file contains the TestClient constructor and stage management.
// Scenario implementations are in:
//   - TestClient_Scenarios_HappyPath.cpp (success paths)
//   - TestClient_Scenarios_Negative.cpp (error/security tests)

#include "../include/req/testclient/TestClient.h"

#include <iostream>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"

namespace req::testclient {

// Constructor
TestClient::TestClient()
    : currentStage_(EClientStage::NotConnected),
      sessionToken_(req::shared::InvalidSessionToken),
      accountId_(0),
      worldId_(0),
      handoffToken_(req::shared::InvalidHandoffToken),
      zoneId_(0),
      selectedCharacterId_(0) {
}

// Stage transition helper
void TestClient::transitionStage(EClientStage newStage, const std::string& context) {
    EClientStage oldStage = currentStage_;
    currentStage_ = newStage;
    
    std::string logMsg = "[CLIENT] Stage: " + stageToString(oldStage) + " ? " + stageToString(newStage);
    if (!context.empty()) {
        logMsg += " (" + context + ")";
    }
    
    req::shared::logInfo("TestClient", logMsg);
    std::cout << logMsg << std::endl;
}

} // namespace req::testclient
