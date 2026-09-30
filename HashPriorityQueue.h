#ifndef HASH_PRIORITY_QUEUE_H
#define HASH_PRIORITY_QUEUE_H

#include "Node.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <stdexcept>

class HashPriorityQueue {
private:
    std::unordered_map<float, std::vector<Node>> spillBuckets;
    std::priority_queue<float, std::vector<float>, std::greater<float>> minHeap;
    size_t queueSize = 0;

public:
    void push(const Node& node) {
        float spill = node.spill;
        auto& bucket = spillBuckets[spill];
        if (bucket.empty()) {
            minHeap.push(spill);
        }
        bucket.push_back(node);
        queueSize++;
    }

    Node pop() {
        if (minHeap.empty()) {
            throw std::runtime_error("Priority queue is empty");
        }

        float minSpill = minHeap.top();
        auto& bucket = spillBuckets[minSpill];
        Node minNode = std::move(bucket.back());
        bucket.pop_back();
        queueSize--;

        if (bucket.empty()) {
            spillBuckets.erase(minSpill);
            minHeap.pop();
        }

        return minNode;
    }

    const Node& top() const {
        if (minHeap.empty()) {
            throw std::runtime_error("Priority queue is empty");
        }
        float minSpill = minHeap.top();
        return spillBuckets.at(minSpill).back();
    }

    bool empty() const {
        return minHeap.empty();
    }

    size_t size() const {
        return queueSize;
    }
};

#endif // HASH_PRIORITY_QUEUE_H