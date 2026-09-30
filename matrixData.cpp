#include "matrixData.h"
#include "dem.h"
#include <fstream>
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_conv.h"

bool MatrixData::fileExists(const std::string& filename) const {
    std::ifstream file(filename);
    return file.good();
}

bool MatrixData::loadFilledDEM(const std::string& filename, std::vector<float>& data) {
    return readTIFFToFloat(filename, data);
}

bool MatrixData::loadDepressionID(const std::string& filename, std::vector<uint32_t>& data) {
    return readTIFFToUInt32(filename, data);
}

bool MatrixData::loadCatchmentID(const std::string& filename, std::vector<uint32_t>& data) {
    return readTIFFToUInt32(filename, data);
}

bool MatrixData::loadFlowDirection(const std::string& filename, std::vector<uint8_t>& data) {
    return readTIFFToUInt8(filename, data);
}

bool MatrixData::saveFilledDEM(const std::string& filename, const std::vector<float>& data) const {
    return writeFloatToTIFF(filename, data);
}

bool MatrixData::saveDepressionID(const std::string& filename, const std::vector<uint32_t>& data) const {
    return writeUInt32ToTIFF(filename, data);
}

bool MatrixData::saveCatchmentID(const std::string& filename, const std::vector<uint32_t>& data) const {
    return writeUInt32ToTIFF(filename, data);
}

bool MatrixData::saveFlowDirection(const std::string& filename, const std::vector<uint8_t>& data) const {
    return writeUInt8ToTIFF(filename, data);
}

bool MatrixData::readTIFFToFloat(const std::string& filename, std::vector<float>& data) {
    GDALAllRegister();

    GDALDataset* poDataset = (GDALDataset*)GDALOpen(filename.c_str(), GA_ReadOnly);
    if (poDataset == nullptr) {
        std::cerr << "无法打开TIFF文件: " << filename << std::endl;
        return false;
    }

    width = poDataset->GetRasterXSize();
    height = poDataset->GetRasterYSize();

    // 复制地理变换到局部变量
    double local_geo_transform[6];
    poDataset->GetGeoTransform(local_geo_transform);
    for (int i = 0; i < 6; i++) {
        geo_transform[i] = local_geo_transform[i];
    }

    GDALRasterBand* poBand = poDataset->GetRasterBand(1);

    data.resize(width * height);
    CPLErr err = poBand->RasterIO(GF_Read, 0, 0, width, height,
        data.data(), width, height, GDT_Float32, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return err == CE_None;
}

bool MatrixData::readTIFFToUInt32(const std::string& filename, std::vector<uint32_t>& data) {
    GDALAllRegister();

    GDALDataset* poDataset = (GDALDataset*)GDALOpen(filename.c_str(), GA_ReadOnly);
    if (poDataset == nullptr) {
        std::cerr << "无法打开TIFF文件: " << filename << std::endl;
        return false;
    }

    width = poDataset->GetRasterXSize();
    height = poDataset->GetRasterYSize();

    // 复制地理变换到局部变量
    double local_geo_transform[6];
    poDataset->GetGeoTransform(local_geo_transform);
    for (int i = 0; i < 6; i++) {
        geo_transform[i] = local_geo_transform[i];
    }

    GDALRasterBand* poBand = poDataset->GetRasterBand(1);

    data.resize(width * height);
    CPLErr err = poBand->RasterIO(GF_Read, 0, 0, width, height,
        data.data(), width, height, GDT_UInt32, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return err == CE_None;
}

bool MatrixData::readTIFFToUInt8(const std::string& filename, std::vector<uint8_t>& data) {
    GDALAllRegister();

    GDALDataset* poDataset = (GDALDataset*)GDALOpen(filename.c_str(), GA_ReadOnly);
    if (poDataset == nullptr) {
        std::cerr << "无法打开TIFF文件: " << filename << std::endl;
        return false;
    }

    width = poDataset->GetRasterXSize();
    height = poDataset->GetRasterYSize();

    // 复制地理变换到局部变量
    double local_geo_transform[6];
    poDataset->GetGeoTransform(local_geo_transform);
    for (int i = 0; i < 6; i++) {
        geo_transform[i] = local_geo_transform[i];
    }

    GDALRasterBand* poBand = poDataset->GetRasterBand(1);

    data.resize(width * height);
    CPLErr err = poBand->RasterIO(GF_Read, 0, 0, width, height,
        data.data(), width, height, GDT_Byte, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return err == CE_None;
}

bool MatrixData::writeFloatToTIFF(const std::string& filename, const std::vector<float>& data) const {
    GDALAllRegister();

    GDALDriver* poDriver = (GDALDriver*)GDALGetDriverByName("GTiff");
    if (poDriver == nullptr) {
        std::cerr << "无法获取GTiff驱动" << std::endl;
        return false;
    }

    char** papszOptions = nullptr;
    GDALDataset* poDataset = poDriver->Create(filename.c_str(), width, height, 1, GDT_Float32, papszOptions);
    if (poDataset == nullptr) {
        std::cerr << "无法创建TIFF文件: " << filename << std::endl;
        return false;
    }

    // 创建非const的地理变换副本
    double local_geo_transform[6];
    for (int i = 0; i < 6; i++) {
        local_geo_transform[i] = geo_transform[i];
    }

    poDataset->SetGeoTransform(local_geo_transform);
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    poBand->SetNoDataValue(-9999.0f);

    CPLErr err = poBand->RasterIO(GF_Write, 0, 0, width, height,
        (void*)data.data(), width, height, GDT_Float32, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return err == CE_None;
}

bool MatrixData::writeUInt32ToTIFF(const std::string& filename, const std::vector<uint32_t>& data) const {
    GDALAllRegister();

    GDALDriver* poDriver = (GDALDriver*)GDALGetDriverByName("GTiff");
    if (poDriver == nullptr) {
        std::cerr << "无法获取GTiff驱动" << std::endl;
        return false;
    }

    char** papszOptions = nullptr;
    GDALDataset* poDataset = poDriver->Create(filename.c_str(), width, height, 1, GDT_UInt32, papszOptions);
    if (poDataset == nullptr) {
        std::cerr << "无法创建TIFF文件: " << filename << std::endl;
        return false;
    }

    // 创建非const的地理变换副本
    double local_geo_transform[6];
    for (int i = 0; i < 6; i++) {
        local_geo_transform[i] = geo_transform[i];
    }

    poDataset->SetGeoTransform(local_geo_transform);
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    poBand->SetNoDataValue(0);

    CPLErr err = poBand->RasterIO(GF_Write, 0, 0, width, height,
        (void*)data.data(), width, height, GDT_UInt32, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return err == CE_None;
}

bool MatrixData::writeUInt8ToTIFF(const std::string& filename, const std::vector<uint8_t>& data) const {
    GDALAllRegister();

    GDALDriver* poDriver = (GDALDriver*)GDALGetDriverByName("GTiff");
    if (poDriver == nullptr) {
        std::cerr << "无法获取GTiff驱动" << std::endl;
        return false;
    }

    char** papszOptions = nullptr;
    GDALDataset* poDataset = poDriver->Create(filename.c_str(), width, height, 1, GDT_Byte, papszOptions);
    if (poDataset == nullptr) {
        std::cerr << "无法创建TIFF文件: " << filename << std::endl;
        return false;
    }

    // 创建非const的地理变换副本
    double local_geo_transform[6];
    for (int i = 0; i < 6; i++) {
        local_geo_transform[i] = geo_transform[i];
    }

    poDataset->SetGeoTransform(local_geo_transform);
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    poBand->SetNoDataValue(0);

    CPLErr err = poBand->RasterIO(GF_Write, 0, 0, width, height,
        (void*)data.data(), width, height, GDT_Byte, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return err == CE_None;
}