#ifndef FLAG_H
#define FLAG_H

#include <vector>

class Flag {
private:
    int width, height;
    std::vector<unsigned char> flags;

public:
    Flag() : width(0), height(0) {}

    void Init(int w, int h) {
        width = w;
        height = h;
        flags.resize(width * height, 0);
    }

    void Free() {
        flags.clear();
        width = height = 0;
    }

    bool IsProcessedDirect(int row, int col) const {
        return flags[row * width + col] != 0;
    }

    void SetFlag(int row, int col) {
        flags[row * width + col] = 1;
    }

    void ClearFlag(int row, int col) {
        flags[row * width + col] = 0;
    }
};

#endif // FLAG_H
