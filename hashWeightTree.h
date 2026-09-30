#ifndef HASH_WEIGHT_TREE_H
#define HASH_WEIGHT_TREE_H

#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

struct HashWeightTreeNode {
    int32_t dep_id = -1;
    int32_t out_row = -1;
    int32_t out_col = -1;
    int32_t parent = -1;
    int32_t level = 0;
    int32_t dfs_in = -1;
    int32_t dfs_out = -1;
    bool is_boundary = false;

    HashWeightTreeNode() = default;
    HashWeightTreeNode(int32_t id, int32_t r, int32_t c)
        : dep_id(id), out_row(r), out_col(c) {}
};

class HashWeightTree {
private:
    std::vector<HashWeightTreeNode> nodes_vec;        // 连续存储，索引 = dep_id - 1
    std::vector<std::vector<int32_t>> level_lists;
    std::unordered_map<int32_t, std::vector<int32_t>> children_map;

    void buildLevelAndDFS();
    void rebuildLevelLists();

public:
    HashWeightTree() = default;

    // 节点操作（支持批量预留）
    void addNode(int32_t dep_id, int32_t out_row, int32_t out_col);
    void setParent(int32_t dep_id, int32_t parent_id);
    void markBoundary(int32_t dep_id);
    void finalize();

    // 批量构建：预留空间
    void reserveNodes(int32_t max_id) { nodes_vec.reserve(max_id); }

    // 查询接口
    const HashWeightTreeNode* getNode(int32_t dep_id) const;
    std::vector<int32_t> getDirectUpstream(int32_t dep_id) const;
    std::vector<int32_t> getDirectDownstream(int32_t dep_id) const;
    std::vector<int32_t> getAllUpstream(int32_t dep_id) const;
    std::vector<int32_t> getAllDownstream(int32_t dep_id) const;
    std::vector<int32_t> getAllDepressionIDs() const;

    // 统计信息
    int getNodeCount() const { return static_cast<int>(nodes_vec.size()); }
    int getBoundaryCount() const;
    int getMaxLevel() const;

    // 序列化
    bool saveToBinaryFile(const std::string& filename) const;
    bool loadFromBinaryFile(const std::string& filename);

    void printStatistics() const;
};

#endif // HASH_WEIGHT_TREE_H