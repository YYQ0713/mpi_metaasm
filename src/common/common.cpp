#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

#include "common.hpp"
using namespace std;


long long detect_available_mem() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        std::cerr << "Failed to open /proc/meminfo" << std::endl;
        return -1;
    }

    std::string line;
    long long totalMem = 0; // Total memory in KB

    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        long long value;
        std::string unit;

        iss >> key >> value >> unit;

        if (key == "MemTotal:") {
            totalMem = value * 1024; // Convert KB to Bytes
            break;
        }
    }

    meminfo.close();
    return totalMem;
}
