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

    // Static buffers for memory reuse - optimized for block processing
    static vector<uint8_t> vis;
    static vector<Rect> rects;
    static vector<Key> to_erase;
    static vector<vector<uint8_t>> tile_ids(parent_y, vector<uint8_t>(parent_x));
    static vector<vector<RLESeg>> row_segs(parent_y);
    static vector<vector<bool>> used(parent_y);
    
    // Pre-allocate memory for better performance and block processing
    rects.reserve(parent_x * parent_y / 4); // Estimate max rectangles per tile
    to_erase.reserve(1 << 16); // Reserve space for stack cleanup
    
    // Pre-reserve space for row segments to avoid reallocations
    for (int i = 0; i < parent_y; ++i) {
        row_segs[i].reserve(parent_x / 2); // Estimate max segments per row
        used[i].reserve(parent_x / 2);
    }

    for (int z = 0; z < Z; ++z)
    {
        if (slab_z_count == 0)
        {
            // New slab: clear stacks to ensure we don't cross parent_z boundaries
            stacks.clear();
        }

        // Process this slice in y-bands of height parent_y - optimized block processing
        for (int y0 = 0; y0 < Y; y0 += parent_y)
        {
            // Initialize buffers for this band - reuse memory efficiently
            bandRows.clear();
            bandRows.resize(parent_y);
            
            // Clear row segments and usage flags efficiently
            for (int i = 0; i < parent_y; ++i) {
                row_segs[i].clear();
                used[i].clear();
            }

            rects.clear();
            to_erase.clear();
            
            // Read exactly parent_y non-empty lines for this band - block read optimization
            for (int yy = 0; yy < parent_y; ++yy)
            {
                if (!read_nonempty_line(bandRows[yy]))
                {
                    cerr << "Unexpected EOF in block data.";
                    return 1;
                }
                
                // Optimize string operations for block processing
                const int current_size = (int)bandRows[yy].size();
                if (current_size != X)
                {
                    if (current_size < X)
                    {
                        // Pad with spaces efficiently
                        bandRows[yy].reserve(X);
                        bandRows[yy].append((size_t)(X - current_size), ' ');
                    }
                    else
                    {
                        // Truncate efficiently
                        bandRows[yy].resize((size_t)X);
                    }
                }
            }

            const int ty = y0 / parent_y; // tile row index

            // For each tile across X - optimized block processing
            for (int tx = 0, x0 = 0; tx < tiles_x; ++tx, x0 += parent_x)
            {
                // Efficiently fill tile_ids with block processing
                for (int ly = 0; ly < parent_y; ++ly) {
                    const string& row = bandRows[ly];
                    uint8_t* tile_row = tile_ids[ly].data();
                    for (int lx = 0; lx < parent_x; ++lx) {
                        tile_row[lx] = get_id((unsigned char)row[x0 + lx]);
                    }
                    row_segs[ly].clear();
                    used[ly].clear();
                }
                rects.clear();

                // Step 1: Optimized RLE for each row using preprocessed label ids
                for (int ly = 0; ly < parent_y; ++ly) {
                    const uint8_t* tile_row = tile_ids[ly].data();
                    int lx = 0;
                    while (lx < parent_x) {
                        uint8_t lab = tile_row[lx];
                        int rx = lx + 1;
                        // Unroll loop for better performance on small tiles
                        while (rx < parent_x && tile_row[rx] == lab) {
                            ++rx;
                        }
                        row_segs[ly].push_back({lx, rx, lab});
                        lx = rx;
                    }
                }

                // Step 2: Optimized vertical merge with early termination
                for (int ly = 0; ly < parent_y; ++ly) {
                    used[ly].assign(row_segs[ly].size(), false);
                }

                for (int ly = 0; ly < parent_y; ++ly) {
                    for (size_t si = 0; si < row_segs[ly].size(); ++si) {
                        if (used[ly][si]) continue;
                        
                        const auto &seg = row_segs[ly][si];
                        int height = 1;
                        int next_ly = ly + 1;
                        
                        // Optimized vertical merging
                        while (next_ly < parent_y) {
                            bool found = false;
                            const auto& next_row_segs = row_segs[next_ly];
                            const auto& next_used = used[next_ly];
                            
                            for (size_t sj = 0; sj < next_row_segs.size(); ++sj) {
                                if (next_used[sj]) continue;
                                
                                const auto &next_seg = next_row_segs[sj];
                                if (next_seg.lx == seg.lx &&
                                    next_seg.rx == seg.rx &&
                                    next_seg.lab == seg.lab) {
                                    found = true;
                                    used[next_ly][sj] = true;
                                    break;
                                }
                            }
                            if (!found) break;
                            ++height;
                            ++next_ly;
                        }
                        
                        used[ly][si] = true;
                        rects.push_back(Rect{(uint16_t)seg.lx, (uint16_t)ly, 
                                           (uint16_t)(seg.rx - seg.lx), (uint16_t)height, seg.lab});
                    }
                }

                // Update stacks for these rectangles at tile (tx,ty) - optimized block processing
                for (const auto &r : rects)
                {
                    Key k{(uint32_t)tx, (uint32_t)ty, r.rx, r.ry, r.dx, r.dy, r.lab};
                    auto result = stacks.try_emplace(k, StackInfo{z, 1, current_slice_tag});
                    if (!result.second)
                    {
                        result.first->second.dz += 1;
                        result.first->second.last_seen = current_slice_tag;
                    }
                }
            }
        }

        // After finishing the whole slice: close stacks that did not continue on this slice
        // Optimized block processing for stack cleanup
        to_erase.clear();
        to_erase.reserve(stacks.size() / 4 + 1);
        
        for (auto it = stacks.begin(); it != stacks.end();) {
            const auto& k = it->first;
            const auto& st = it->second;
            
            if (st.last_seen != current_slice_tag) {
                // emit and mark for erase
                int gx = k.tx * parent_x + k.rx;
                int gy = k.ty * parent_y + k.ry;
                emit_block(gx, gy, st.z_start, k.dx, k.dy, st.dz, k.lab);
                it = stacks.erase(it); // Efficient erase using iterator
            } else {
                ++it;
            }
        }

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
