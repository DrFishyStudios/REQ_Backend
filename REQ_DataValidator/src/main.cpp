#include <exception>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../include/req/datavalidator/DataValidator.h"

int main(int argc, char* argv[]) {
    try {
        req::shared::initLogger("REQ_DataValidator");

        // For now we use default roots; later we could allow overrides via args.
        auto result = req::datavalidator::runAllValidations();

        if (result.success) {
            req::shared::logInfo("Main", "REQ_DataValidator completed successfully.");
            return 0;
        } else {
            req::shared::logError("Main",
                "REQ_DataValidator completed with errors. Failing passes: " +
                std::to_string(result.errorCount));
            return 1;
        }
    } catch (const std::exception& e) {
        req::shared::logError("Main",
            std::string{"Unhandled exception in REQ_DataValidator: "} + e.what());
        return 1;
    }
}
