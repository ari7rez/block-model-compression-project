// this is the last code i got up an running
// Fast streaming compressor with strict parent-boundary clamping.
// Emits: x,y,z,dx,dy,dz,label
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstdint>

using namespace std;

// ---------- tiny utils ----------
static inline string rtrim_cr(const string &s)
{
    if (!s.empty() && s.back() == '\r')
        return s.substr(0, s.size() - 1);
    return s;
}
static inline string trim(const string &s)
{
    size_t i = 0, j = s.size();
    while (i < j && (unsigned char)s[i] <= 32)
        ++i;
    while (j > i && (unsigned char)s[j - 1] <= 32)
        --j;
    return s.substr(i, j - i);
}
static inline bool next_row(string &out)
{
    while (getline(cin, out))
    {
        out = rtrim_cr(out);
        if (!out.empty())
            return true; // non-empty row
        // blank lines act as slice separators; skip
    }
    return false;
}

// ---------- keys & hashes ----------
struct KeyY
{
    int x0, x1, lab, ytile;
    bool operator==(const KeyY &o) const
    {
        return x0 == o.x0 && x1 == o.x1 && lab == o.lab && ytile == o.ytile;
    }
};
struct KeyZ
{
    int x0, x1, y0, y1, lab, ztile;
    bool operator==(const KeyZ &o) const
    {
        return x0 == o.x0 && x1 == o.x1 && y0 == o.y0 && y1 == o.y1 && lab == o.lab && ztile == o.ztile;
    }
};
struct KeyYHash
{
    size_t operator()(const KeyY &k) const noexcept
    {
        using std::uint64_t;
        // simple 64-bit mix
        uint64_t h = 1469598103934665603ULL;
        auto mix = [&](uint64_t v)
        { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
        mix((unsigned)k.x0);
        mix((unsigned)k.x1);
        mix((unsigned)k.lab);
        mix((unsigned)k.ytile);
        return (size_t)h;
    }
};
struct KeyZHash
{
    size_t operator()(const KeyZ &k) const noexcept
    {
        using std::uint64_t;
        uint64_t h = 1469598103934665603ULL;
        auto mix = [&](uint64_t v)
        { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
        mix((unsigned)k.x0);
        mix((unsigned)k.x1);
        mix((unsigned)k.y0);
        mix((unsigned)k.y1);
        mix((unsigned)k.lab);
        mix((unsigned)k.ztile);
        return (size_t)h;
    }
};

// Rectangle in one z-slice (already clamped to ytile and x parent)
struct Rect2D
{
    int x0, x1, y0, y1, lab;
};

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // --- 1) Header ---
    string line, header;
    while (getline(cin, header))
    {
        header = trim(header);
        if (!header.empty())
            break;
    }
    if (header.empty())
        return 1;

    for (char &c : header)
        if (c == ',')
            c = ' ';
    long long XL, YL, ZL, pxL, pyL, pzL;
    {
        stringstream ss(header);
        if (!(ss >> XL >> YL >> ZL >> pxL >> pyL >> pzL))
            return 1;
    }
    const int X = (int)XL, Y = (int)YL, Z = (int)ZL;
    const int px = (int)pxL, py = (int)pyL, pz = (int)pzL;

    // --- 2) Tag -> label (dedupe to ints for speed) ---
    int tagToLab[256];
    for (int i = 0; i < 256; ++i)
        tagToLab[i] = -1;
    vector<string> labels;
    labels.reserve(32);
    unordered_map<string, int> labelIdByText;
    labelIdByText.reserve(32);

    while (getline(cin, line))
    {
        string t = trim(rtrim_cr(line));
        if (t.empty())
            break; // end of tag table
        size_t p = t.find(',');
        if (p == string::npos || p == 0)
            return 1;
        string tagstr = trim(t.substr(0, p));
        string label = trim(t.substr(p + 1));
        unsigned char tag = (unsigned char)tagstr[0];
        int lid;
        auto it = labelIdByText.find(label);
        if (it == labelIdByText.end())
        {
            lid = (int)labels.size();
            labels.push_back(label);
            labelIdByText.emplace(label, lid);
        }
        else
        {
            lid = it->second;
        }
        tagToLab[tag] = lid;
    }

    // --- 3) Streaming compression with parent clamps ---
    vector<int> rowIds;
    rowIds.resize(X);
    unordered_map<KeyY, int, KeyYHash> openY;
    openY.reserve(1024);
    vector<Rect2D> rectsZ;
    rectsZ.reserve((size_t)Y * 4);

    unordered_map<KeyZ, int, KeyZHash> openZ;
    openZ.reserve(4096);
    unordered_map<KeyZ, int, KeyZHash> nextOpenZ;
    nextOpenZ.reserve(4096);

    for (int z = 0; z < Z; ++z)
    {
        openY.clear();
        rectsZ.clear();

        for (int y = 0; y < Y; ++y)
        {
            if (!next_row(line))
                return 1;
            if ((int)line.size() < X)
                return 1;

            // translate row to label ids
            for (int x = 0; x < X; ++x)
            {
                unsigned char c = (unsigned char)line[(size_t)x];
                int lid = tagToLab[c];
                if (lid < 0)
                    return 1; // unknown tag
                rowIds[x] = lid;
            }

            const int ytile = y / py;

            // For present detection this row:
            unordered_map<KeyY, char, KeyYHash> present; // value unused
            present.reserve(64);

            // Walk X with parent-boundary clamping
            int x = 0;
            while (x < X)
            {
                const int lid = rowIds[x];
                const int xParentEnd = (x / px) * px + (px - 1);
                int x1 = x;
                // extend run but never beyond parent end or row end
                while (x1 + 1 < X && (x1 + 1) <= xParentEnd && rowIds[x1 + 1] == lid)
                    ++x1;

                KeyY k{x, x1, lid, ytile};
                present.emplace(k, 0);

                auto it = openY.find(k);
                if (it == openY.end())
                {
                    openY.emplace(k, y); // open at this y
                } // else: keep open

                x = x1 + 1; // next segment (if natural run spills, new parent starts a new segment)
            }

            // Close any openY not present in this row
            if (!openY.empty())
            {
                vector<KeyY> toClose;
                toClose.reserve(openY.size());
                for (const auto &kv : openY)
                {
                    if (kv.first.ytile != ytile || present.find(kv.first) == present.end())
                    {
                        Rect2D r;
                        r.x0 = kv.first.x0;
                        r.x1 = kv.first.x1;
                        r.y0 = kv.second;
                        r.y1 = y - 1;
                        r.lab = kv.first.lab;
                        rectsZ.push_back(r);
                        toClose.push_back(kv.first);
                    }
                }
                for (const auto &k : toClose)
                    openY.erase(k);
            }
        }

        // Close remaining Y-rectangles at end of slice
        for (const auto &kv : openY)
        {
            Rect2D r;
            r.x0 = kv.first.x0;
            r.x1 = kv.first.x1;
            r.y0 = kv.second;
            r.y1 = Y - 1;
            r.lab = kv.first.lab;
            rectsZ.push_back(r);
        }
        openY.clear();

        // --- Merge across Z within ztile ---
        nextOpenZ.clear();
        nextOpenZ.reserve(rectsZ.size() * 2 + 1);
        const int ztile = z / pz;

        for (const auto &r : rectsZ)
        {
            // Sanity clamp (should already hold): dx<=px, dy<=py
            int dx = r.x1 - r.x0 + 1;
            int dy = r.y1 - r.y0 + 1;
            if (dx < 1 || dy < 1 || dx > px || dy > py)
            {
                // If this ever triggers, it indicates a logic error; bail to be safe.
                return 1;
            }

            KeyZ kz{r.x0, r.x1, r.y0, r.y1, r.lab, ztile};
            auto it = openZ.find(kz);
            if (it != openZ.end())
            {
                // extend existing prism: keep original z0
                nextOpenZ.emplace(kz, it->second);
            }
            else
            {
                // start new prism at this z
                nextOpenZ.emplace(kz, z);
            }
        }

        // Emit prisms that ended at z-1
        for (const auto &kv : openZ)
        {
            if (nextOpenZ.find(kv.first) == nextOpenZ.end())
            {
                const KeyZ &k = kv.first;
                const int z0 = kv.second;
                const int dx = k.x1 - k.x0 + 1;
                const int dy = k.y1 - k.y0 + 1;
                const int dz = (z - 1) - z0 + 1;
                // ensure dz within parent
                if (dz < 1 || dz > pz)
                    return 1;
                cout << k.x0 << ',' << k.y0 << ',' << z0 << ','
                     << dx << ',' << dy << ',' << dz << ','
                     << labels[k.lab] << '\n';
            }
        }

        openZ.swap(nextOpenZ);
    }

    // --- Flush remaining prisms at final z ---
    for (const auto &kv : openZ)
    {
        const KeyZ &k = kv.first;
        const int z0 = kv.second;
        const int dx = k.x1 - k.x0 + 1;
        const int dy = k.y1 - k.y0 + 1;
        const int dz = (Z - 1) - z0 + 1;
        if (dz < 1 || dz > pz)
            return 1;
        cout << k.x0 << ',' << k.y0 << ',' << z0 << ','
             << dx << ',' << dy << ',' << dz << ','
             << labels[k.lab] << '\n';
    }

    return 0;
}