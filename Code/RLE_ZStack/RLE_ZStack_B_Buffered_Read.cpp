#include <cstdio>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <algorithm>
using namespace std;

// ------------------------------------------------------------
// Block Model Compression — Per-slice RLE Rectangles + Z-stacking
// - Streams input by z-slice, in y-bands of parent_y rows
// - Builds 2D rectangles per parent tile on each slice via greedy flood (grow-rectangle)
// - Z-stacks identical rectangles (same footprint+label) up to parent_z thickness
// - Respects parent boundaries; emits: x,y,z,dx,dy,dz,label
// ------------------------------------------------------------


struct Rect
{
    uint16_t rx, ry, dx, dy; // rectangle within parent tile (relative x,y in [0..parent_x), [0..parent_y))
    uint8_t lab;             // label id (0..255)
};

struct Key
{
    uint32_t tx, ty;         // tile indices (x tile, y tile)
    uint16_t rx, ry, dx, dy; // rect footprint within tile
    uint8_t lab;

    bool operator==(const Key &o) const noexcept
    {
        return tx == o.tx && ty == o.ty && rx == o.rx && ry == o.ry && dx == o.dx && dy == o.dy && lab == o.lab;
    }
};

struct KeyHash
{
    size_t operator()(Key const &k) const noexcept
    {
        // Simple 64-bit hash combine
        uint64_t h = 1469598103934665603ull; // FNV offset
        auto mix = [&](uint64_t v)
        { h ^= v; h *= 1099511628211ull; };
        mix(k.tx);
        mix(k.ty);
        mix(((uint64_t)k.rx << 48) ^ ((uint64_t)k.ry << 32) ^ ((uint64_t)k.dx << 16) ^ k.dy);
        mix(k.lab);
        return (size_t)h;
    }
};

struct StackInfo
{
    int z_start;   // starting z of this stack
    int dz;        // current height
    int last_seen; // slice index tag to detect continuation within current slice
};

struct RLESeg {
    int lx, rx; // [lx, rx)
    uint8_t lab;
};

static inline string trim(const string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a]))
        ++a;
    while (b > a && isspace((unsigned char)s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

static inline void split_csv(const string &s, vector<string> &out)
{
    out.clear();
    string cur;
    for (char c : s)
    {
        if (c == ',')
        {
            out.push_back(trim(cur));
            cur.clear();
        }
        else
            cur.push_back(c);
    }
    out.push_back(trim(cur));
}

// Reads next non-empty line from cin (skips empty lines). Returns false on EOF.
static bool read_nonempty_line(string &line)
{
    while (true)
    {
        if (!std::getline(cin, line))
            return false;
        if (!line.empty())
            return true; // skip blank separator lines
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;
    auto t0 = chrono::steady_clock::now();
    char* data = (char*)mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    volatile uint64_t sum = 0;
    for (size_t i = 0; i < filesize; ++i) sum += data[i];
    auto t1 = chrono::steady_clock::now();
    munmap(data, filesize);
    close(fd);
    double sec = chrono::duration<double>(t1 - t0).count();
    cout << "Read " << filesize / 1e6 << " MB in " << sec << " s, speed: " << (filesize / 1e6 / sec) << " MB/s\n";
    return 0;
}
