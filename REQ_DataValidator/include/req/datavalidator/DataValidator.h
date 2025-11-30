#pragma once

#include <string>
#include <cstddef>

namespace req::datavalidator {

struct ValidationResult {
    bool success{true};
    std::size_t errorCount{0};
    std::size_t warningCount{0};
};

// Run all validation passes and return aggregated result.
ValidationResult runAllValidations(
    const std::string& configRoot = "config",
    const std::string& accountsRoot = "data/accounts",
    const std::string& charactersRoot = "data/characters");

} // namespace req::datavalidator
