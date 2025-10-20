#include "SysCalls.h"
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

bool SysCalls::execute(const char* cmd) {
	int result = system(cmd);
	return WEXITSTATUS(result) == 0;
}

bool SysCalls::execute(const std::string cmd) {
    return execute(cmd.c_str());
}

std::string SysCalls::executeAndCatchOutput(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);

    // Sanitize output by removing newlines and carriage returns
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.cend());
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());

    return result;
}

std::string SysCalls::executeAndCatchOutput(const std::string cmd) {
    return executeAndCatchOutput(cmd.c_str());
}

std::vector<std::string> SysCalls::executeAndCatchOutputCsv(const char* cmd) {
    std::vector<std::string> result;
    std::string output = executeAndCatchOutput(cmd);
    return split(output, ',');
}

std::vector<std::string> SysCalls::executeAndCatchOutputCsv(const std::string cmd) {
    return executeAndCatchOutputCsv(cmd.c_str());
}

std::vector<std::string> SysCalls::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}