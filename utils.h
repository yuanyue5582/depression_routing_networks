#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

// 工具函数
inline void setNoData(float* data, int size, float noDataValue) {
    for (int i = 0; i < size; i++) {
        data[i] = noDataValue;
    }
}

// 目录操作
bool createDirectory(const std::string& path);
bool fileExists(const std::string& filename);
std::string getFileNameWithoutExtension(const std::string& path);

#endif // UTILS_H
