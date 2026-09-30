#ifndef FILL_DEM_HEADER_H
#define FILL_DEM_HEADER_H

#include "hashWeightTree.h"
#include <vector>
#include <string>
#include <cstdint>
#include <limits>

struct OutletInfo {
    int32_t dep_id;
    int32_t row;
    int32_t col;
    int32_t parent;   // -1 表示边界或无父洼地
};

class FillDEM {
private:
    int width = 0, height = 0;
    float no_data_value = -9999.0f;
    float cell_width = 1.0f, cell_height = 1.0f;
    HashWeightTree tree;
    std::vector<OutletInfo> pending_outlets;

    static constexpr uint32_t BOUNDARY_LABEL = std::numeric_limits<uint32_t>::max();

public:
    FillDEM() = default;
    bool process(const std::string& input_dem, const std::string& output_dir);
    const HashWeightTree& getTree() const { return tree; }

private:
    bool findValidBoundaries(const std::vector<float>& dem_data,
                             std::vector<std::pair<int,int>>& boundaries);
    void fillDEM_Barnes(const std::vector<float>& dem_data,
                        std::vector<uint32_t>& depression_matrix,
                        std::vector<uint32_t>& catchment_matrix);

    inline int Get_rowTo(int dir, int row) const {
        static const int row_offset[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        return row + row_offset[dir];
    }
    inline int Get_colTo(int dir, int col) const {
        static const int col_offset[8] = {1, 1, 0, -1, -1, -1, 0, 1};
        return col + col_offset[dir];
    }
    inline bool is_InGrid(int row, int col) const {
        return row >= 0 && row < height && col >= 0 && col < width;
    }
    inline bool is_NoData(float value) const {
        return value <= no_data_value + 1.0f;
    }
};

#endif // FILL_DEM_HEADER_H