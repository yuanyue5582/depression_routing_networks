#include "hashWeightTree.h"
#include <fstream>
#include <algorithm>
#include <stack>
#include <iostream>
#include <cstring>

#pragma pack(push, 1)
struct TreeNodeBinary {
    int32_t dep_id;
    int32_t out_row, out_col;
    int32_t parent;
    int32_t level;
    int32_t dfs_in;
    int32_t dfs_out;
    uint8_t is_boundary;
};

struct TreeFileHeader {
    char magic[16];
    int32_t version;
    int32_t node_count;
    int32_t max_level;
    int32_t boundary_count;
    int32_t total_levels;
};
#pragma pack(pop)

void HashWeightTree::addNode(int32_t dep_id, int32_t out_row, int32_t out_col) {
    // 确保 vector 足够大（索引 = dep_id - 1）
    size_t idx = static_cast<size_t>(dep_id - 1);
    if (idx >= nodes_vec.size()) {
        nodes_vec.resize(idx + 1);
    }
    nodes_vec[idx] = HashWeightTreeNode(dep_id, out_row, out_col);
}

void HashWeightTree::setParent(int32_t dep_id, int32_t parent_id) {
    size_t idx = static_cast<size_t>(dep_id - 1);
    if (idx < nodes_vec.size() && nodes_vec[idx].dep_id == dep_id) {
        nodes_vec[idx].parent = parent_id;
    }
}

void HashWeightTree::markBoundary(int32_t dep_id) {
    size_t idx = static_cast<size_t>(dep_id - 1);
    if (idx < nodes_vec.size() && nodes_vec[idx].dep_id == dep_id) {
        auto& node = nodes_vec[idx];
        node.is_boundary = true;
        node.parent = -1;
        node.level = 1;
    }
}

void HashWeightTree::finalize() {
    buildLevelAndDFS();
    rebuildLevelLists();
}

void HashWeightTree::buildLevelAndDFS() {
    children_map.clear();
    for (const auto& node : nodes_vec) {
        if (node.dep_id == -1) continue;
        int32_t pid = node.parent;
        if (pid != -1) {
            children_map[pid].push_back(node.dep_id);
        }
    }
    for (auto& pair : children_map) {
        std::sort(pair.second.begin(), pair.second.end());
    }

    // 标记边界洼地
    for (auto& node : nodes_vec) {
        if (node.dep_id == -1) continue;
        if (node.parent == -1 || node.is_boundary) {
            node.is_boundary = true;
            node.parent = -1;
            node.level = 1;
        }
    }

    // DFS 计算 dfs_in/dfs_out 和层级
    int dfs_counter = 0;
    for (auto& node : nodes_vec) {
        if (node.dep_id == -1) continue;
        if (node.is_boundary && node.dfs_in == -1) {
            std::stack<std::pair<int32_t, bool>> stk;
            stk.push({node.dep_id, false});
            while (!stk.empty()) {
                auto [cur_id, visited] = stk.top();
                stk.pop();
                auto& cur = nodes_vec[cur_id - 1];
                if (!visited) {
                    cur.dfs_in = dfs_counter++;
                    stk.push({cur_id, true});
                    auto it = children_map.find(cur_id);
                    if (it != children_map.end()) {
                        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
                            auto& child = nodes_vec[*rit - 1];
                            child.level = cur.level + 1;
                            stk.push({*rit, false});
                        }
                    }
                } else {
                    cur.dfs_out = dfs_counter - 1;
                }
            }
        }
    }
}

void HashWeightTree::rebuildLevelLists() {
    int max_level = 0;
    for (const auto& node : nodes_vec) {
        if (node.dep_id != -1 && node.level > max_level) max_level = node.level;
    }
    level_lists.clear();
    level_lists.resize(max_level);
    for (const auto& node : nodes_vec) {
        if (node.dep_id == -1) continue;
        int lvl = node.level;
        if (lvl >= 1 && lvl <= max_level) {
            level_lists[lvl - 1].push_back(node.dep_id);
        }
    }
    for (auto& lst : level_lists) {
        std::sort(lst.begin(), lst.end());
    }
}

const HashWeightTreeNode* HashWeightTree::getNode(int32_t dep_id) const {
    size_t idx = static_cast<size_t>(dep_id - 1);
    if (idx < nodes_vec.size() && nodes_vec[idx].dep_id == dep_id) {
        return &nodes_vec[idx];
    }
    return nullptr;
}

std::vector<int32_t> HashWeightTree::getDirectUpstream(int32_t dep_id) const {
    std::vector<int32_t> res;
    auto it = children_map.find(dep_id);
    if (it != children_map.end()) {
        res = it->second;
        std::sort(res.begin(), res.end());
    }
    return res;
}

std::vector<int32_t> HashWeightTree::getDirectDownstream(int32_t dep_id) const {
    std::vector<int32_t> res;
    const auto* node = getNode(dep_id);
    if (node && node->parent != -1) {
        res.push_back(node->parent);
    }
    return res;
}

std::vector<int32_t> HashWeightTree::getAllUpstream(int32_t dep_id) const {
    std::vector<int32_t> res;
    const auto* node = getNode(dep_id);
    if (!node || node->dfs_in == -1) return res;
    for (const auto& other : nodes_vec) {
        if (other.dep_id == -1 || other.dep_id == dep_id) continue;
        if (other.level > node->level &&
            other.dfs_in >= node->dfs_in && other.dfs_out <= node->dfs_out) {
            res.push_back(other.dep_id);
        }
    }
    std::sort(res.begin(), res.end());
    return res;
}

std::vector<int32_t> HashWeightTree::getAllDownstream(int32_t dep_id) const {
    std::vector<int32_t> res;
    const auto* node = getNode(dep_id);
    if (!node || node->is_boundary) return res;
    int32_t current = node->parent;
    while (current != -1) {
        res.push_back(current);
        const auto* next_node = getNode(current);
        if (!next_node || next_node->is_boundary) break;
        current = next_node->parent;
    }
    return res;
}

std::vector<int32_t> HashWeightTree::getAllDepressionIDs() const {
    std::vector<int32_t> ids;
    for (const auto& node : nodes_vec) {
        if (node.dep_id != -1) ids.push_back(node.dep_id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

int HashWeightTree::getBoundaryCount() const {
    int cnt = 0;
    for (const auto& node : nodes_vec) {
        if (node.dep_id != -1 && node.is_boundary) cnt++;
    }
    return cnt;
}

int HashWeightTree::getMaxLevel() const {
    int ml = 0;
    for (const auto& node : nodes_vec) {
        if (node.dep_id != -1 && node.level > ml) ml = node.level;
    }
    return ml;
}

bool HashWeightTree::saveToBinaryFile(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    TreeFileHeader header;
    memcpy(header.magic, "HASH_TREE_V8", 12);
    header.version = 8;
    header.node_count = getNodeCount();
    header.max_level = getMaxLevel();
    header.boundary_count = getBoundaryCount();
    header.total_levels = static_cast<int32_t>(level_lists.size());
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // 写入所有有效节点（按 dep_id 顺序）
    for (const auto& node : nodes_vec) {
        if (node.dep_id == -1) continue;
        TreeNodeBinary bin;
        bin.dep_id = node.dep_id;
        bin.out_row = node.out_row;
        bin.out_col = node.out_col;
        bin.parent = node.parent;
        bin.level = node.level;
        bin.dfs_in = node.dfs_in;
        bin.dfs_out = node.dfs_out;
        bin.is_boundary = node.is_boundary ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&bin), sizeof(TreeNodeBinary));
    }

    for (const auto& lst : level_lists) {
        int32_t sz = lst.size();
        file.write(reinterpret_cast<const char*>(&sz), sizeof(int32_t));
        if (!lst.empty())
            file.write(reinterpret_cast<const char*>(lst.data()), sz * sizeof(int32_t));
    }
    file.close();
    return true;
}

bool HashWeightTree::loadFromBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    TreeFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (memcmp(header.magic, "HASH_TREE_V8", 12) != 0) return false;

    nodes_vec.clear();
    nodes_vec.reserve(header.node_count);
    for (int i = 0; i < header.node_count; ++i) {
        TreeNodeBinary bin;
        file.read(reinterpret_cast<char*>(&bin), sizeof(TreeNodeBinary));
        HashWeightTreeNode node;
        node.dep_id = bin.dep_id;
        node.out_row = bin.out_row;
        node.out_col = bin.out_col;
        node.parent = bin.parent;
        node.level = bin.level;
        node.dfs_in = bin.dfs_in;
        node.dfs_out = bin.dfs_out;
        node.is_boundary = (bin.is_boundary != 0);
        // 确保 vector 大小足够
        size_t idx = static_cast<size_t>(node.dep_id - 1);
        if (idx >= nodes_vec.size()) nodes_vec.resize(idx + 1);
        nodes_vec[idx] = node;
    }

    level_lists.resize(header.total_levels);
    for (int i = 0; i < header.total_levels; ++i) {
        int32_t sz;
        file.read(reinterpret_cast<char*>(&sz), sizeof(int32_t));
        level_lists[i].resize(sz);
        if (sz > 0)
            file.read(reinterpret_cast<char*>(level_lists[i].data()), sz * sizeof(int32_t));
    }

    // 重建 children_map
    children_map.clear();
    for (const auto& node : nodes_vec) {
        if (node.dep_id != -1 && node.parent != -1) {
            children_map[node.parent].push_back(node.dep_id);
        }
    }
    for (auto& pair : children_map) std::sort(pair.second.begin(), pair.second.end());
    return true;
}

void HashWeightTree::printStatistics() const {
    std::cout << "=== HashWeightTree Statistics ===\n";
    std::cout << "Number of depressions: " << getNodeCount() << "\n";
    std::cout << "Boundary depressions: " << getBoundaryCount() << "\n";
    std::cout << "Max level: " << getMaxLevel() << "\n";
    std::cout << "Level lists size: " << level_lists.size() << "\n";
}