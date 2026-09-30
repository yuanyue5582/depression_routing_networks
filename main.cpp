#include <iostream>
#include <string>
#include <vector>
#include "utils.h"
#include "fillDEM.h"
#include "hashWeightTree.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include <iomanip>

// 模块2：查询洼地树（修改版：显示高程，限制上游输出数量）
void module2(const std::string& tree_path, const std::string& original_dem_path);

// 模块3：生成洼地溢出口高程 DEM
void module3(const std::string& original_dem_path,
             const std::string& tree_path,
             const std::string& output_dem_path);

int main() {
    GDALAllRegister();

    std::string input_file = "/mnt/d/GIS_Data/cook_dem_3m_m.tif";
    std::string base_output_dir = "/mnt/d/basin_outfile/output";
    std::string dem_name = getFileNameWithoutExtension(input_file);
    std::string output_dir = base_output_dir + "/" + dem_name;
    createDirectory(output_dir);

    std::cout << "Select module:\n"
              << "1 - Module 1 (Barnes fill + tree build)\n"
              << "2 - Module 2 (Query tree)\n"
              << "3 - Module 3 (Export outlet DEM)\n";
    int choice;
    std::cin >> choice;

    if (choice == 1) {
        FillDEM filler;
        if (!filler.process(input_file, output_dir)) {
            std::cerr << "Module 1 failed\n";
            return -1;
        }
        std::cout << "Module 1 completed. Results saved to " << output_dir << std::endl;
    } else if (choice == 2) {
        std::string tree_path = output_dir + "/hash_weight_tree.bin";
        module2(tree_path, input_file);   // 传入原始DEM路径
    } else if (choice == 3) {
        std::string tree_path = output_dir + "/hash_weight_tree.bin";
        std::string outlet_dem_path = output_dir + "/outlet_dem.tif";
        module3(input_file, tree_path, outlet_dem_path);
    } else {
        std::cerr << "Invalid choice\n";
        return -1;
    }
    return 0;
}

// =========== 模块2实现（高精度坐标输出）===========
void module2(const std::string& tree_path, const std::string& original_dem_path) {
    // 1. 加载洼地树
    HashWeightTree tree;
    if (!tree.loadFromBinaryFile(tree_path)) {
        std::cerr << "Failed to load tree file: " << tree_path << std::endl;
        return;
    }
    int total_deps = tree.getNodeCount();
    std::cout << "Tree loaded. " << total_deps << " depressions.\n";

    // --- 新增统计信息 ---
    std::vector<int32_t> dep_ids = tree.getAllDepressionIDs();
    int rootCount = 0, leafCount = 0;
    for (int32_t id : dep_ids) {
        const auto* node = tree.getNode(id);
        if (!node) continue;
        // 根节点：parent == -1（包括边界洼地）
        if (node->parent == -1 || node->is_boundary) {
            rootCount++;
        }
        // 叶子节点：没有上游（子节点）的节点
        auto children = tree.getDirectUpstream(id);
        if (children.empty()) {
            leafCount++;
        }
    }
    std::cout << "Root count: " << rootCount << std::endl;
    std::cout << "Leaf count: " << leafCount << std::endl;
    std::cout << "Max level: " << tree.getMaxLevel() << std::endl;

    // 2. 读取原始DEM，获取地理变换参数和大小
    GDALDataset* poSrcDS = (GDALDataset*)GDALOpen(original_dem_path.c_str(), GA_ReadOnly);
    if (!poSrcDS) {
        std::cerr << "Failed to open original DEM: " << original_dem_path << std::endl;
        return;
    }
    int width = poSrcDS->GetRasterXSize();
    int height = poSrcDS->GetRasterYSize();

    double geoTransform[6];
    if (poSrcDS->GetGeoTransform(geoTransform) != CE_None) {
        std::cerr << "Warning: Failed to get GeoTransform, using default identity.\n";
        geoTransform[0] = 0.0; geoTransform[1] = 1.0; geoTransform[2] = 0.0;
        geoTransform[3] = 0.0; geoTransform[4] = 0.0; geoTransform[5] = 1.0;
    }

    const char* projection = poSrcDS->GetProjectionRef();
    bool isGeographic = (projection && strstr(projection, "GEOGCS") != nullptr);

    // 读取高程数据（用于溢出口高程）
    GDALRasterBand* poBand = poSrcDS->GetRasterBand(1);
    int bHasNoData = 0;
    double srcNoData = poBand->GetNoDataValue(&bHasNoData);
    float noDataVal = (bHasNoData) ? static_cast<float>(srcNoData) : -9999.0f;

    std::vector<float> dem_data(width * height);
    CPLErr err = poBand->RasterIO(GF_Read, 0, 0, width, height,
                                  dem_data.data(), width, height,
                                  GDT_Float32, 0, 0);
    if (err != CE_None) {
        std::cerr << "Failed to read DEM data." << std::endl;
        GDALClose(poSrcDS);
        return;
    }
    GDALClose(poSrcDS);

    auto getElevation = [&](int row, int col) -> float {
        if (row < 0 || row >= height || col < 0 || col >= width) return noDataVal;
        return dem_data[row * width + col];
    };

    auto rowColToXY = [&](int row, int col, double& x, double& y) {
        x = geoTransform[0] + col * geoTransform[1] + row * geoTransform[2];
        y = geoTransform[3] + col * geoTransform[4] + row * geoTransform[5];
    };

    std::cout << "DEM coordinate system: " << (isGeographic ? "Geographic (lon/lat)" : "Projected (easting/northing)") << std::endl;
    std::cout << "GeoTransform: [" << geoTransform[0] << ", " << geoTransform[1] << ", " << geoTransform[2] << ", "
              << geoTransform[3] << ", " << geoTransform[4] << ", " << geoTransform[5] << "]" << std::endl;

    int32_t dep_id;
    while (true) {
        std::cout << "\n[Total depressions: " << total_deps << "] Enter depression ID (1 ~ " << total_deps << ", 0 to exit): ";
        std::cin >> dep_id;
        if (dep_id == 0) break;

        const HashWeightTreeNode* node = tree.getNode(dep_id);
        if (!node) {
            std::cout << "Depression ID " << dep_id << " not found.\n";
            continue;
        }

        double mapX, mapY;
        rowColToXY(node->out_row, node->out_col, mapX, mapY);
        float outlet_elev = getElevation(node->out_row, node->out_col);

        std::cout << "--- Information for depression " << dep_id << " ---\n";
        std::cout << "Outlet cell: (row=" << node->out_row << ", col=" << node->out_col << ")\n";
        // 使用 fixed 和 setprecision(7) 输出7位小数
        std::cout << std::fixed << std::setprecision(7);
        std::cout << "Outlet map coordinates: (" << mapX << ", " << mapY << ")\n";
        if (isGeographic) {
            std::cout << "   (Longitude = " << mapX << ", Latitude = " << mapY << ")\n";
        } else {
            std::cout << "   (Easting = " << mapX << ", Northing = " << mapY << ")\n";
        }
        std::cout << std::resetiosflags(std::ios::fixed);  // 恢复默认格式
        std::cout << "Outlet elevation: " << outlet_elev << "\n";
        std::cout << "Parent (downstream): " << node->parent << "\n";
        std::cout << "Level: " << node->level << "\n";
        std::cout << "Is boundary: " << (node->is_boundary ? "Yes" : "No") << "\n";
        std::cout << "DFS interval: [" << node->dfs_in << ", " << node->dfs_out << "]\n";

        // 打印上下游信息（限长输出）
        auto direct_up = tree.getDirectUpstream(dep_id);
        auto direct_down = tree.getDirectDownstream(dep_id);
        auto all_up = tree.getAllUpstream(dep_id);
        auto all_down = tree.getAllDownstream(dep_id);

        auto printLimited = [](const std::vector<int32_t>& vec, size_t max_show = 10) {
            if (vec.empty()) { std::cout << "(none)"; return; }
            if (vec.size() <= max_show) {
                for (size_t i = 0; i < vec.size(); ++i) {
                    if (i > 0) std::cout << " ";
                    std::cout << vec[i];
                }
            } else {
                for (size_t i = 0; i < max_show; ++i) {
                    if (i > 0) std::cout << " ";
                    std::cout << vec[i];
                }
                std::cout << " ... (and " << (vec.size() - max_show) << " more)";
            }
        };

        std::cout << "Direct upstream (" << direct_up.size() << "): "; printLimited(direct_up); std::cout << "\n";
        std::cout << "Direct downstream (" << direct_down.size() << "): "; printLimited(direct_down); std::cout << "\n";
        std::cout << "All upstream (" << all_up.size() << "): "; printLimited(all_up); std::cout << "\n";
        std::cout << "All downstream (" << all_down.size() << "): "; printLimited(all_down); std::cout << std::endl;
    }
}

// ===================== 模块3实现（新增出口ID栅格） =====================
void module3(const std::string& original_dem_path,
             const std::string& tree_path,
             const std::string& output_dem_path) {
    std::cout << "=== Module 3: Export outlet DEM ===" << std::endl;

    // 1. 读取原始DEM
    GDALDataset* poSrcDS = (GDALDataset*)GDALOpen(original_dem_path.c_str(), GA_ReadOnly);
    if (!poSrcDS) {
        std::cerr << "Failed to open original DEM: " << original_dem_path << std::endl;
        return;
    }
    int width = poSrcDS->GetRasterXSize();
    int height = poSrcDS->GetRasterYSize();
    double geoTransform[6];
    poSrcDS->GetGeoTransform(geoTransform);
    const char* pszProjection = poSrcDS->GetProjectionRef();

    GDALRasterBand* poBand = poSrcDS->GetRasterBand(1);
    int bHasNoData = 0;
    double srcNoData = poBand->GetNoDataValue(&bHasNoData);
    float noDataVal = (bHasNoData) ? static_cast<float>(srcNoData) : -9999.0f;

    std::vector<float> dem_data(width * height);
    CPLErr err = poBand->RasterIO(GF_Read, 0, 0, width, height,
                                  dem_data.data(), width, height,
                                  GDT_Float32, 0, 0);
    if (err != CE_None) {
        std::cerr << "Failed to read DEM data." << std::endl;
        GDALClose(poSrcDS);
        return;
    }
    GDALClose(poSrcDS);
    std::cout << "Original DEM loaded: " << width << " x " << height << std::endl;

    // 2. 加载洼地树
    HashWeightTree tree;
    if (!tree.loadFromBinaryFile(tree_path)) {
        std::cerr << "Failed to load tree file: " << tree_path << std::endl;
        return;
    }
    std::cout << "Tree loaded. Number of depressions: " << tree.getNodeCount() << std::endl;

    // 3. 创建输出栅格（高程）
    std::vector<float> outlet_dem(width * height, noDataVal);
    // 4. 创建输出栅格（洼地ID），初始为0（作为NoData）
    std::vector<uint32_t> outlet_id(width * height, 0);

    // 5. 遍历所有洼地，填充出口高程和ID
    std::vector<int32_t> dep_ids = tree.getAllDepressionIDs();
    int valid_outlet_count = 0;
    for (int32_t dep_id : dep_ids) {
        const HashWeightTreeNode* node = tree.getNode(dep_id);
        if (!node) continue;
        int row = node->out_row;
        int col = node->out_col;
        if (row < 0 || row >= height || col < 0 || col >= width) {
            std::cerr << "Warning: depression " << dep_id
                      << " has invalid outlet coordinates (" << row << "," << col << ")" << std::endl;
            continue;
        }
        int idx = row * width + col;
        float elev = dem_data[idx];
        if (bHasNoData && (elev <= noDataVal + 1.0f)) {
            std::cerr << "Warning: depression " << dep_id
                      << " outlet is NoData at (" << row << "," << col << ")" << std::endl;
            continue;
        }
        outlet_dem[idx] = elev;
        outlet_id[idx] = static_cast<uint32_t>(dep_id);
        valid_outlet_count++;
    }
    std::cout << "Valid outlets written: " << valid_outlet_count << std::endl;

    // 6. 输出高程GeoTIFF
    GDALDriver* poDriver = (GDALDriver*)GDALGetDriverByName("GTiff");
    if (!poDriver) {
        std::cerr << "GTiff driver not available." << std::endl;
        return;
    }
    char** papszOptions = nullptr;
    // 高程文件
    GDALDataset* poDstDS = poDriver->Create(output_dem_path.c_str(),
                                            width, height, 1,
                                            GDT_Float32, papszOptions);
    if (!poDstDS) {
        std::cerr << "Failed to create output file: " << output_dem_path << std::endl;
        return;
    }
    poDstDS->SetGeoTransform(geoTransform);
    poDstDS->SetProjection(pszProjection);
    GDALRasterBand* poOutBand = poDstDS->GetRasterBand(1);
    poOutBand->SetNoDataValue(noDataVal);
    err = poOutBand->RasterIO(GF_Write, 0, 0, width, height,
                              outlet_dem.data(), width, height,
                              GDT_Float32, 0, 0);
    GDALClose(poDstDS);
    if (err == CE_None) {
        std::cout << "Outlet DEM (elevation) saved to: " << output_dem_path << std::endl;
    } else {
        std::cerr << "Failed to write outlet DEM (elevation)." << std::endl;
    }

    // 7. 输出洼地ID GeoTIFF
    std::string output_id_path = output_dem_path.substr(0, output_dem_path.find_last_of('.')) + "_id.tif";
    GDALDataset* poIdDS = poDriver->Create(output_id_path.c_str(),
                                           width, height, 1,
                                           GDT_UInt32, papszOptions);
    if (!poIdDS) {
        std::cerr << "Failed to create output ID file: " << output_id_path << std::endl;
        return;
    }
    poIdDS->SetGeoTransform(geoTransform);
    poIdDS->SetProjection(pszProjection);
    GDALRasterBand* poIdBand = poIdDS->GetRasterBand(1);
    poIdBand->SetNoDataValue(0);   // 0 表示非洼地出口
    err = poIdBand->RasterIO(GF_Write, 0, 0, width, height,
                             outlet_id.data(), width, height,
                             GDT_UInt32, 0, 0);
    GDALClose(poIdDS);
    if (err == CE_None) {
        std::cout << "Outlet DEM (depression ID) saved to: " << output_id_path << std::endl;
    } else {
        std::cerr << "Failed to write outlet DEM (ID)." << std::endl;
    }
}