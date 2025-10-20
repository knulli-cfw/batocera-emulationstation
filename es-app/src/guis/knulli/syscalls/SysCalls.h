#pragma once
#include <vector>
#include <string>

class SysCalls
{
public:
        static bool execute(const char* cmd);
        static bool execute(const std::string cmd);
        static std::string executeAndCatchOutput(const char* cmd);
        static std::string executeAndCatchOutput(const std::string cmd);
        static std::vector<std::string> executeAndCatchOutputCsv(const char* cmd);
        static std::vector<std::string> executeAndCatchOutputCsv(const std::string cmd);

private:
        static std::vector<std::string> split(const std::string& str, char delimiter);

};
