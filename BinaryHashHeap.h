#ifndef BINARY_HASH_HEAP_H
#define BINARY_HASH_HEAP_H

#include "Node.h"
#include "HashPriorityQueue.h"
#include <vector>
#include <algorithm>      // std::nth_element
#include <stdexcept>
#include <utility>        // std::move

class BinaryHashHeap {
private:
    HashPriorityQueue heap;          // 存储较小的一半（≤ median）
    std::vector<Node> buffer;        // 存储较大的一半（> median），无序
    float median;                    // 当前分界值
    bool medianInitialized;          // 是否已初始化

    // 重平衡：从 buffer 中取出较小的一半移入堆，更新中位数
    void rebalance() {
        if (buffer.empty()) {
            medianInitialized = false;
            return;
        }

        size_t n = buffer.size();
        size_t k = n / 2;            // 较小的一半数量（下中位数）
        if (k == 0) k = 1;           // 至少保证一个元素移入堆，避免死循环

        // 特殊处理 n == 1：直接移入堆
        if (n == 1) {
            median = buffer[0].spill;
            heap.push(buffer[0]);
            buffer.clear();
            medianInitialized = true;
            return;
        }

        // 使用 nth_element 进行部分排序（平均 O(n)）
        // 使前 k 个元素均 ≤ 第 k 个元素（即 buffer[k]）
        std::nth_element(buffer.begin(), buffer.begin() + k, buffer.end(),
                         [](const Node& a, const Node& b) {
                             return a.spill < b.spill;
                         });

        // 中位数取第 k 个元素的值（第 k+1 小），作为分界点
        median = buffer[k].spill;
        medianInitialized = true;

        // 将前 k 个元素（较小的一半）移动到临时 vector
        std::vector<Node> toHeap;
        toHeap.reserve(k);
        toHeap.insert(toHeap.end(),
                      std::make_move_iterator(buffer.begin()),
                      std::make_move_iterator(buffer.begin() + k));

        // 剩余元素构成新的 buffer
        std::vector<Node> newBuffer;
        newBuffer.reserve(n - k);
        newBuffer.insert(newBuffer.end(),
                         std::make_move_iterator(buffer.begin() + k),
                         std::make_move_iterator(buffer.end()));
        buffer.swap(newBuffer);

        // 批量插入哈希堆（移动语义，零拷贝）
        heap.pushBatch(std::move(toHeap));
    }

public:
    BinaryHashHeap() : medianInitialized(false) {}

    // 插入节点
    void push(const Node& node) {
        if (!medianInitialized) {
            buffer.push_back(node);          // 未初始化，暂存缓冲
        } else {
            if (node.spill <= median) {
                heap.push(node);             // 较小一半入堆
            } else {
                buffer.push_back(node);      // 较大一半入缓冲
            }
        }
    }

    // 弹出最小值（全局最小）
    Node pop() {
        if (empty()) {
            throw std::runtime_error("BinaryHashHeap is empty");
        }
        // 若堆空但缓冲非空，则从缓冲补充较小的一半
        if (heap.empty() && !buffer.empty()) {
            rebalance();
        }
        if (heap.empty()) {
            throw std::runtime_error("BinaryHashHeap is empty");
        }
        return heap.pop();
    }

    // 查看最小值（不弹出）
    const Node& top() {
        if (empty()) {
            throw std::runtime_error("BinaryHashHeap is empty");
        }
        if (heap.empty() && !buffer.empty()) {
            rebalance();
        }
        if (heap.empty()) {
            throw std::runtime_error("BinaryHashHeap is empty");
        }
        return heap.top();
    }

    bool empty() const {
        return heap.empty() && buffer.empty();
    }

    size_t size() const {
        return heap.size() + buffer.size();
    }
};

#endif // BINARY_HASH_HEAP_H