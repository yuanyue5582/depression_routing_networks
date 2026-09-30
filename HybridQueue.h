#ifndef HYBRID_QUEUE_HEADER_H
#define HYBRID_QUEUE_HEADER_H

#include "Node.h"
#include "Flag.h"
#include "HashPriorityQueue.h"
#include <queue>

class HybridQueue {
private:
    HashPriorityQueue _priority_queue;
    std::queue<Node> _plain_queue;
    Flag _in_flag;
    int width, height;

public:
    HybridQueue(int w, int h) : width(w), height(h) {
        _in_flag.Init(width, height);
    }

    ~HybridQueue() {
        _in_flag.Free();
    }

    bool isQueued(int row, int col) {
        return _in_flag.IsProcessedDirect(row, col);
    }

    void pushPriority(const Node& item) {
        _priority_queue.push(item);
        _in_flag.SetFlag(item.row, item.col);
    }

    void pushPlain(const Node& item) {
        _plain_queue.push(item);
        _in_flag.SetFlag(item.row, item.col);
    }

    Node popCell() {
        if (!_plain_queue.empty()) {
            Node item = _plain_queue.front();
            _plain_queue.pop();
            return item;
        }
        else if (!_priority_queue.empty()) {
            Node item = _priority_queue.pop();
            return item;
        }
        return Node(); // 返回空节点
    }

    bool notEmpty() {
        return !_plain_queue.empty() || !_priority_queue.empty();
    }
};

#endif // HYBRID_QUEUE_HEADER_H