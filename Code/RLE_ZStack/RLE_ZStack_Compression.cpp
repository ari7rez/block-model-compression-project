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

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ---------------- Header ----------------
    string header;
    if (!read_nonempty_line(header))
        return 0;
    vector<string> parts;
    split_csv(header, parts);
    if (parts.size() != 6)
    {
        cerr << "Bad header. Expected 6 comma-separated integers.\n";
        return 1;
    }
    long long X = stoll(parts[0]);
    long long Y = stoll(parts[1]);
    long long Z = stoll(parts[2]);
    int parent_x = stoi(parts[3]);
    int parent_y = stoi(parts[4]);
    int parent_z = stoi(parts[5]);

    if (X <= 0 || Y <= 0 || Z <= 0 || parent_x <= 0 || parent_y <= 0 || parent_z <= 0)
    {
        cerr << "Header values must be positive.\n";
        return 1;
    }
    if (X % parent_x || Y % parent_y || Z % parent_z)
    {
        cerr << "Dimensions must be divisible by parent sizes (per spec).\n";
        return 1;
    }

    // ---------------- Tag table ----------------
    // Map input char -> label id; and id -> label string
    array<uint8_t, 256> char2id;
    char2id.fill(255);
    array<string, 256> id2label; // we'll fill only used ids
    uint8_t next_id = 0;

    string line;
    while (std::getline(cin, line))
    {
        if (line.size() == 0)
            break; // blank line ends tag table
        string s = trim(line);
        if (s.empty())
            break;
        auto pos = s.find(',');
        if (pos == string::npos)
        {
            cerr << "Bad tag table line: " << s << "\n";
            return 1;
        }
        string tag_str = trim(s.substr(0, pos));
        string lab_str = trim(s.substr(pos + 1));
        if (tag_str.empty() || lab_str.empty())
        {
            cerr << "Bad tag table line: " << s << "\n";
            return 1;
        }
        unsigned char tag_ch = (unsigned char)tag_str[0];
        if (char2id[tag_ch] != 255)
        { /* duplicate tag */
        }
        char2id[tag_ch] = next_id;
        id2label[next_id] = lab_str;
        next_id++;
        if (next_id == 0)
        {
            cerr << "Too many labels ( >256 ).\n";
            return 1;
        }
    }

    // Helper to convert a model character to label id
    auto get_id = [&](unsigned char ch) -> uint8_t
    {
        uint8_t id = char2id[ch];
        if (id == 255)
        {
            // Unknown tag - treat as its own new label if space remains (robustness)
            if (next_id == 0)
            {
                cerr << "Tag table overflow.";
                exit(1);
            }
            char2id[ch] = next_id;
            id2label[next_id] = string(1, (char)ch);
            id = next_id;
            next_id++;
        }
        return id;
    };

    const int tiles_x = (int)(X / parent_x);
    const int tiles_y = (int)(Y / parent_y);

    // Global active stacks across ALL tiles within current Z-slab (thickness parent_z)
    unordered_map<Key, StackInfo, KeyHash> stacks;
    stacks.reserve(1 << 20);

    auto emit_block = [&](int gx, int gy, int gz, int dx, int dy, int dz, uint8_t lab)
    {
        // Output: x,y,z,dx,dy,dz,label
        cout << gx << ',' << gy << ',' << gz << ',' << dx << ',' << dy << ',' << dz << ',' << id2label[lab] << '\n';
    };

    int slab_z_count = 0;      // slices processed within current slab
    int current_slice_tag = 1; // increases each slice to mark "seen"

    // Buffer for a band of rows (parent_y rows) for the current slice
    vector<string> bandRows;
    bandRows.reserve(parent_y);

    for (int z = 0; z < Z; ++z)
    {
        if (slab_z_count == 0)
        {
            // New slab: clear stacks to ensure we don't cross parent_z boundaries
            stacks.clear();
        }

        // Process this slice in y-bands of height parent_y
        for (int y0 = 0; y0 < Y; y0 += parent_y)
        {
            bandRows.clear();
            bandRows.resize(parent_y);
            // Read exactly parent_y non-empty lines for this band
            for (int yy = 0; yy < parent_y; ++yy)
            {
                if (!read_nonempty_line(bandRows[yy]))
                {
                    cerr << "Unexpected EOF in block data.";
                    return 1;
                }
                if ((int)bandRows[yy].size() != X)
                {
                    // Be tolerant: if shorter, pad; if longer, truncate
                    if ((int)bandRows[yy].size() < X)
                        bandRows[yy].append((size_t)(X - bandRows[yy].size()), ' ');
                    else
                        bandRows[yy].resize((size_t)X);
                }
            }

            const int ty = y0 / parent_y; // tile row index

            // For each tile across X
            for (int tx = 0, x0 = 0; tx < tiles_x; ++tx, x0 += parent_x)
            {
                // Build rectangles for this tile on this slice using greedy grow (within parent_x * parent_y)
                const int W = parent_x, H = parent_y;
                // visited flags for current tile area
                static vector<uint8_t> vis;
                vis.assign(W * H, 0);

                auto at_id = [&](int lx, int ly) -> uint8_t
                {
                    // local coords within tile: lx in [0..W-1], ly in [0..H-1]
                    unsigned char ch = (unsigned char)bandRows[ly][x0 + lx];
                    return get_id(ch);
                };

                vector<Rect> rects;
                rects.reserve(W * H / 2 + 1);

                for (int ly = 0; ly < H; ++ly)
                {
                    for (int lx = 0; lx < W; ++lx)
                    {
                        int idx = ly * W + lx;
                        if (vis[idx])
                            continue;
                        uint8_t lab = at_id(lx, ly);
                        // Determine maximal width from (lx,ly)
                        int w = 0;
                        while (lx + w < W)
                        {
                            int idx2 = ly * W + (lx + w);
                            if (vis[idx2])
                                break;
                            if (at_id(lx + w, ly) != lab)
                                break;
                            ++w;
                        }
                        // Determine maximal height such that every row fully supports width w
                        int h = 1;
                        bool ok = true;
                        for (int ly2 = ly + 1; ly2 < H; ++ly2)
                        {
                            for (int k = 0; k < w; ++k)
                            {
                                int idx3 = ly2 * W + (lx + k);
                                if (vis[idx3] || at_id(lx + k, ly2) != lab)
                                {
                                    ok = false;
                                    break;
                                }
                            }
                            if (!ok)
                                break;
                            else
                                ++h;
                        }
                        // mark visited and add rect
                        for (int yy2 = 0; yy2 < h; ++yy2)
                        {
                            for (int xx2 = 0; xx2 < w; ++xx2)
                            {
                                vis[(ly + yy2) * W + (lx + xx2)] = 1;
                            }
                        }
                        rects.push_back(Rect{(uint16_t)lx, (uint16_t)ly, (uint16_t)w, (uint16_t)h, lab});
                    }
                }

                // Update stacks for these rectangles at tile (tx,ty)
                for (const auto &r : rects)
                {
                    Key k{(uint32_t)tx, (uint32_t)ty, r.rx, r.ry, r.dx, r.dy, r.lab};
                    auto it = stacks.find(k);
                    if (it == stacks.end())
                    {
                        stacks.emplace(k, StackInfo{z, 1, current_slice_tag});
                    }
                    else
                    {
                        it->second.dz += 1;
                        it->second.last_seen = current_slice_tag;
                    }
                }
            }
        }

        // After finishing the whole slice: close stacks that did not continue on this slice
        // We must iterate over all and emit where last_seen != current_slice_tag
        vector<Key> to_erase;
        to_erase.reserve(stacks.size() / 4 + 1);
        for (auto const &kv : stacks)
        {
            auto const &k = kv.first;
            auto const &st = kv.second;
            if (st.last_seen != current_slice_tag)
            {
                // emit and mark for erase
                int gx = k.tx * parent_x + k.rx;
                int gy = k.ty * parent_y + k.ry;
                emit_block(gx, gy, st.z_start, k.dx, k.dy, st.dz, k.lab);
                to_erase.push_back(k);
            }
        }
        for (auto const &k : to_erase)
            stacks.erase(k);

        // Prepare for next slice in slab
        ++current_slice_tag;
        ++slab_z_count;

        if (slab_z_count == parent_z)
        {
            // Flush any remaining stacks (cannot cross parent_z boundary)
            for (auto const &kv : stacks)
            {
                auto const &k = kv.first;
                auto const &st = kv.second;
                int gx = k.tx * parent_x + k.rx;
                int gy = k.ty * parent_y + k.ry;
                emit_block(gx, gy, st.z_start, k.dx, k.dy, st.dz, k.lab);
            }
            stacks.clear();
            slab_z_count = 0;
        }
    }

    return 0;
}
