#pragma once

#include "Diagnostics.h"

bool Tokenize(const std::string& Source, const fs::path& File, std::vector<FToken>& OutTokens, FDiagnostics& Diag);