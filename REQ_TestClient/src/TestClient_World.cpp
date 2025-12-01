#include "../include/req/testclient/TestClient.h"

#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"

#include <string>
#include <vector>
#include <iostream>

namespace req::testclient {

// World-related helper functions
// 
// Note: In the current protocol, world selection is integrated into the login flow.
// The LoginResponse includes a list of available worlds, and the test client
// automatically selects the first one. If more sophisticated world selection
// is needed in the future (e.g., user prompt, multi-world support), 
// world-specific helpers can be added here.
//
// For now, this file serves as a placeholder for future world-related functionality.

} // namespace req::testclient
