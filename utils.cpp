#include "utils.h"
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

bool createDirectory(const std::string& path) {
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    // Linux/Unix: 使用 mkdir，权限 0755
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

std::string getFileNameWithoutExtension(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    std::string fileName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    size_t lastDot = fileName.find_last_of(".");
    return (lastDot == std::string::npos) ? fileName : fileName.substr(0, lastDot);
}