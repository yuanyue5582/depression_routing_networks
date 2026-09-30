#include "fillDEM.h"
#include "matrixData.h"
#include "utils.h"
#include "HashPriorityQueue.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <queue>
#include <vector>
#include <cstring>
#include <limits>
#include "gdal_priv.h"

// ============================================================
// 主处理流程
// ============================================================
bool FillDEM::process(const std::string& input_dem, const std::string& output_dir) {
    std::cout << "=== Module 1: Barnes Fill (No DEM modification) + Tree Building ===" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    // 1. 读取DEM
    GDALAllRegister();
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(input_dem.c_str(), GA_ReadOnly);
    if (!poDataset) {
        std::cerr << "无法打开DEM文件: " << input_dem << std::endl;
        return false;
    }
    width = poDataset->GetRasterXSize();
    height = poDataset->GetRasterYSize();
    double geo_transform[6];
    poDataset->GetGeoTransform(geo_transform);
    cell_width = std::abs(static_cast<float>(geo_transform[1]));
    cell_height = std::abs(static_cast<float>(geo_transform[5]));

    std::vector<float> dem_data(width * height);
    GDALRasterBand* poBand = poDataset->GetRasterBand(1);
    int pbSuccess = 0;
    double dfNoData = poBand->GetNoDataValue(&pbSuccess);
    if (pbSuccess) no_data_value = static_cast<float>(dfNoData);
    poBand->RasterIO(GF_Read, 0, 0, width, height,
                     dem_data.data(), width, height, GDT_Float32, 0, 0);
    GDALClose((GDALDatasetH)poDataset);
    std::cout << "DEM尺寸: " << width << " x " << height << std::endl;

    // 2. 初始化矩阵（无流向）
    std::vector<uint32_t> depression_matrix(width * height, 0);
    std::vector<uint32_t> catchment_matrix(width * height, 0);

    // 3. 执行填洼算法
    auto phase1_start = std::chrono::high_resolution_clock::now();
    std::cout << "执行Barnes填洼算法（记录洼地、集水区，边界标签内部为最大值，输出时转为0）..." << std::endl;
    fillDEM_Barnes(dem_data, depression_matrix, catchment_matrix);
    auto phase1_end = std::chrono::high_resolution_clock::now();
    auto phase1_ms = std::chrono::duration_cast<std::chrono::milliseconds>(phase1_end - phase1_start).count();
    std::cout << "第一阶段耗时: " << phase1_ms << " ms" << std::endl;

    // 4. 构建树
    auto build_start = std::chrono::high_resolution_clock::now();
    std::cout << "从洼地出口信息构建树节点及关系..." << std::endl;

    int32_t max_dep_id = 0;
    for (const auto& info : pending_outlets) {
        if (info.dep_id > max_dep_id) max_dep_id = info.dep_id;
    }
    tree.reserveNodes(max_dep_id);
    for (const auto& info : pending_outlets) {
        tree.addNode(info.dep_id, info.row, info.col);
        if (info.parent != -1) {
            tree.setParent(info.dep_id, info.parent);
        } else {
            tree.markBoundary(info.dep_id);
        }
    }
    tree.finalize();

    auto build_end = std::chrono::high_resolution_clock::now();
    auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start).count();
    std::cout << "构建树节点及关系耗时: " << build_ms << " ms" << std::endl;

    // 5. 保存结果（集水区矩阵中的边界标签已转换为0）
    if (!createDirectory(output_dir)) {
        std::cerr << "无法创建输出目录: " << output_dir << std::endl;
        return false;
    }
    MatrixData mat;
    mat.setDimensions(width, height);
    mat.setGeoTransform(geo_transform);

    std::string dep_id_path = output_dir + "/depression_id.tif";
    std::string catch_path = output_dir + "/catchment_areas.tif";
    std::string tree_path = output_dir + "/hash_weight_tree.bin";

    if (!mat.saveDepressionID(dep_id_path, depression_matrix)) {
        std::cerr << "保存洼地ID矩阵失败!" << std::endl;
        return false;
    }
    if (!mat.saveCatchmentID(catch_path, catchment_matrix)) {
        std::cerr << "保存集水区矩阵失败!" << std::endl;
        return false;
    }
    std::cout << "洼地ID矩阵已保存: " << dep_id_path << std::endl;
    std::cout << "集水区矩阵已保存: " << catch_path << std::endl;

    if (!tree.saveToBinaryFile(tree_path)) {
        std::cerr << "保存哈希权重树失败!" << std::endl;
        return false;
    }
    std::cout << "哈希权重树已保存: " << tree_path << std::endl;

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "\n模块1完成！" << std::endl;
    std::cout << "洼地数量: " << tree.getNodeCount() << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;

    return true;
}

// ============================================================
// Barnes填洼算法（含局部队列LQ，无流向，输出前转换边界标签为0）
// ============================================================
void FillDEM::fillDEM_Barnes(const std::vector<float>& dem_data,
                             std::vector<uint32_t>& depression_matrix,
                             std::vector<uint32_t>& catchment_matrix) {
    HashPriorityQueue priority_queue;
    std::queue<Node> plain_queue;
    std::vector<uint8_t> processed(width * height, 0);

    // 边界初始化
    std::vector<std::pair<int, int>> boundaries;
    findValidBoundaries(dem_data, boundaries);
    for (const auto& boundary : boundaries) {
        int row = boundary.first, col = boundary.second;
        if (!processed[row * width + col]) {
            priority_queue.push(Node(row, col, dem_data[row * width + col]));
        }
    }
    std::cout << "边界单元格数: " << boundaries.size() << std::endl;

    int processed_cells = 0;
    int created_depressions = 0;
    int next_dep_id = 1;          // 洼地ID从1开始，边界标签为最大值
    pending_outlets.clear();

    while (!priority_queue.empty()) {
        Node current_node = priority_queue.pop();
        int c_row = current_node.row;
        int c_col = current_node.col;
        float c_value = current_node.spill;
        int c_idx = c_row * width + c_col;

        if (processed[c_idx]) continue;

        // ---- 获取当前标签，若无则标记为边界 ----
        uint32_t current_label = 0;
        if (depression_matrix[c_idx] != 0) {
            current_label = depression_matrix[c_idx];
        } else if (catchment_matrix[c_idx] != 0) {
            current_label = catchment_matrix[c_idx];
        } else {
            catchment_matrix[c_idx] = BOUNDARY_LABEL;
            current_label = BOUNDARY_LABEL;
        }
        int current_dep_id = (current_label != BOUNDARY_LABEL && depression_matrix[c_idx] != 0) ?
                             static_cast<int>(depression_matrix[c_idx]) : 0;

        // ---- 局部普通队列 LQ ----
        std::vector<Node> local_queue;

        // 遍历八个邻居
        for (int i = 0; i < 8; i++) {
            int n_row = Get_rowTo(i, c_row);
            int n_col = Get_colTo(i, c_col);
            if (!is_InGrid(n_row, n_col)) continue;
            int n_idx = n_row * width + n_col;
            if (processed[n_idx]) continue;

            float neighbor_value = dem_data[n_idx];
            if (is_NoData(neighbor_value)) continue;

            // --- 邻居更低：洼地扩展或创建 ---
            if (neighbor_value < c_value) {
                // 如果当前单元格还不是洼地，创建新洼地
                if (current_dep_id == 0) {
                    current_dep_id = next_dep_id++;
                    created_depressions++;
                    depression_matrix[c_idx] = current_dep_id;
                    catchment_matrix[c_idx] = current_dep_id;
                    current_label = current_dep_id;

                    // ---- 覆盖 LQ 中所有节点的集水区标签 ----
                    for (auto& node : local_queue) {
                        int idx = node.row * width + node.col;
                        catchment_matrix[idx] = current_dep_id;
                    }

                    // ---- 确定父级洼地 ----
                    int32_t parent_id = -1;
                    float min_elev = std::numeric_limits<float>::max();
                    for (int j = 0; j < 8; j++) {
                        int p_row = Get_rowTo(j, c_row);
                        int p_col = Get_colTo(j, c_col);
                        if (!is_InGrid(p_row, p_col)) continue;
                        int p_idx = p_row * width + p_col;
                        if (!processed[p_idx]) continue;
                        uint32_t label = depression_matrix[p_idx];
                        if (label == 0) label = catchment_matrix[p_idx];
                        // 排除边界标签和自身
                        if (label != 0 && label != BOUNDARY_LABEL && label != static_cast<uint32_t>(current_dep_id)) {
                            float elev = dem_data[p_idx];
                            if (elev < min_elev) {
                                min_elev = elev;
                                parent_id = static_cast<int32_t>(label);
                            }
                        }
                    }
                    pending_outlets.push_back({current_dep_id, c_row, c_col, parent_id});
                }

                // 将更低邻居加入普通队列（洼地内部）
                if (depression_matrix[n_idx] == 0) {
                    depression_matrix[n_idx] = current_dep_id;
                    catchment_matrix[n_idx] = current_dep_id;
                    plain_queue.push(Node(n_row, n_col, c_value));
                }
            }
            // --- 邻居更高或相等：标记集水区并加入优先队列 ---
            else {
                // 仅当邻居没有任何标签时
                if (depression_matrix[n_idx] == 0 && catchment_matrix[n_idx] == 0) {
                    // 标记为当前标签
                    catchment_matrix[n_idx] = current_label;
                    // 将其加入 LQ
                    local_queue.push_back(Node(n_row, n_col, neighbor_value));
                    // 压入优先队列
                    priority_queue.push(Node(n_row, n_col, neighbor_value));
                }
                // 已有标签则不重复入队
            }
        } // end for neighbors

        // 当前单元格处理完毕
        processed[c_idx] = 1;
        processed_cells++;

        // ========== 处理普通队列中所有洼地内部单元格 ==========
        while (!plain_queue.empty()) {
            Node plain_node = plain_queue.front();
            plain_queue.pop();
            int p_row = plain_node.row;
            int p_col = plain_node.col;
            float p_spill = plain_node.spill;
            int p_idx = p_row * width + p_col;

            if (processed[p_idx]) continue;

            int p_dep_id = depression_matrix[p_idx];
            processed_cells++;

            for (int i = 0; i < 8; i++) {
                int n_row = Get_rowTo(i, p_row);
                int n_col = Get_colTo(i, p_col);
                if (!is_InGrid(n_row, n_col)) continue;
                int n_idx = n_row * width + n_col;
                if (processed[n_idx]) continue;

                float neighbor_value = dem_data[n_idx];
                if (is_NoData(neighbor_value)) continue;

                if (neighbor_value < p_spill) {
                    // 更低，继续扩展同一洼地
                    if (depression_matrix[n_idx] == 0) {
                        depression_matrix[n_idx] = p_dep_id;
                        catchment_matrix[n_idx] = p_dep_id;
                        plain_queue.push(Node(n_row, n_col, p_spill));
                    }
                } else {
                    // 更高或相等，标记集水区并入优先队列（仅当无标签）
                    if (depression_matrix[n_idx] == 0 && catchment_matrix[n_idx] == 0) {
                        catchment_matrix[n_idx] = p_dep_id;
                        priority_queue.push(Node(n_row, n_col, neighbor_value));
                    }
                }
            }

            processed[p_idx] = 1;
        } // while plain_queue

    } // while priority_queue

    // ---- 输出前将边界标签（最大值）转换为 0 ----
    for (auto& val : catchment_matrix) {
        if (val == BOUNDARY_LABEL) val = 0;
    }

    std::cout << "\n处理完成！" << std::endl;
    std::cout << "总处理单元格: " << processed_cells << std::endl;
    std::cout << "创建洼地数: " << created_depressions << std::endl;
}

// ============================================================
// 边界单元格查找（不变）
// ============================================================
bool FillDEM::findValidBoundaries(const std::vector<float>& dem_data,
                                  std::vector<std::pair<int,int>>& boundaries) {
    boundaries.clear();
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (is_NoData(dem_data[row * width + col])) continue;
            bool isBoundary = false;
            for (int i = 0; i < 8; i++) {
                int nRow = Get_rowTo(i, row);
                int nCol = Get_colTo(i, col);
                if (!is_InGrid(nRow, nCol) || is_NoData(dem_data[nRow * width + nCol])) {
                    isBoundary = true;
                    break;
                }
            }
            if (isBoundary) boundaries.emplace_back(row, col);
        }
    }
    return !boundaries.empty();
}