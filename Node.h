#ifndef NODE_H
#define NODE_H

struct Node {
    int row = -1;
    int col = -1;
    float spill = 0.0f;

    Node(int r = -1, int c = -1, float s = 0.0f) : row(r), col(c), spill(s) {}

    // 用于优先队列的比较
    bool operator>(const Node& other) const {
        return spill > other.spill;
    }
};

#endif // NODE_H
