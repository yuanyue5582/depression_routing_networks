#ifndef MATRIX_DATA_H
#define MATRIX_DATA_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <string>

// 矩阵数据管理类
class MatrixData {
private:
    int width = 0;
    int height = 0;
    double geo_transform[6] = { 0, 0, 0, 0, 0, 0 };

public:
    MatrixData() = default;

    // 加载各种矩阵数据
    bool loadFilledDEM(const std::string& filename, std::vector<float>& data);
    bool loadDepressionID(const std::string& filename, std::vector<uint32_t>& data);
    bool loadCatchmentID(const std::string& filename, std::vector<uint32_t>& data);
    bool loadFlowDirection(const std::string& filename, std::vector<uint8_t>& data);

    // 保存矩阵数据
    bool saveFilledDEM(const std::string& filename, const std::vector<float>& data) const;
    bool saveDepressionID(const std::string& filename, const std::vector<uint32_t>& data) const;
    bool saveCatchmentID(const std::string& filename, const std::vector<uint32_t>& data) const;
    bool saveFlowDirection(const std::string& filename, const std::vector<uint8_t>& data) const;

    // 获取地理信息
    const double* getGeoTransform() const { return geo_transform; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // 辅助函数
    bool fileExists(const std::string& filename) const;

   void setDimensions(int w, int h) { width = w; height = h; }
   void setGeoTransform(const double* gt) { for (int i = 0; i < 6; ++i) geo_transform[i] = gt[i]; }

private:
    bool readTIFFToFloat(const std::string& filename, std::vector<float>& data);
    bool readTIFFToUInt32(const std::string& filename, std::vector<uint32_t>& data);
    bool readTIFFToUInt8(const std::string& filename, std::vector<uint8_t>& data);

    bool writeFloatToTIFF(const std::string& filename, const std::vector<float>& data) const;
    bool writeUInt32ToTIFF(const std::string& filename, const std::vector<uint32_t>& data) const;
    bool writeUInt8ToTIFF(const std::string& filename, const std::vector<uint8_t>& data) const;
};

#endif // MATRIX_DATA_H
