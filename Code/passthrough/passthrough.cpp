#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <cstdint>

/** this code just read input and send output no comparison  */
using namespace std;

static inline string trim(const string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;

    // 1) Read header (x_count, y_count, z_count, parent_x, parent_y, parent_z)
    //    Skip leading blank lines just in case.
    string header;
    while (getline(cin, header))
    {
        header = trim(header);
        if (!header.empty())
            break;
    }
    if (header.empty())
        return 0;

    // Parse 6 comma-separated integers
    for (char &ch : header)
        if (ch == ',')
            ch = ' ';
    stringstream hs(header);
    long long x_count, y_count, z_count, parent_x, parent_y, parent_z;
    if (!(hs >> x_count >> y_count >> z_count >> parent_x >> parent_y >> parent_z))
    {
        cerr << "Invalid header\n";
        return 1;
    }

    // 2) Read tag table: lines "tag,label" until a blank line
    unordered_map<char, string> label_of;
    while (getline(cin, line))
    {
        string t = trim(line);
        if (t.empty())
            break; // end of tag table
        auto pos = t.find(',');
        if (pos == string::npos || pos == 0)
        {
            cerr << "Invalid tag table line: " << t << "\n";
            return 1;
        }
        string tag_str = trim(t.substr(0, pos));
        string label = trim(t.substr(pos + 1));
        if (tag_str.empty())
        {
            cerr << "Empty tag\n";
            return 1;
        }
        char tag = tag_str[0];
        label_of[tag] = label;
    }

    // 3) Stream block data: z slices, each with y_count rows of x_count chars.
    auto next_nonempty_line = [&](string &dst) -> bool
    {
        while (getline(cin, dst))
        {
            // do not trim inside rows (labels may depend on exact chars), just strip trailing CR
            if (!dst.empty() && dst.back() == '\r')
                dst.pop_back();
            if (!dst.empty())
                return true;
            // blank lines act as slice separators; keep skipping
        }
        return false;
    };

    for (long long z = 0; z < z_count; ++z)
    {
        for (long long y = 0; y < y_count; ++y)
        {
            if (!next_nonempty_line(line))
            {
                cerr << "Unexpected end of input while reading slice " << z << ", row " << y << "\n";
                return 1;
            }
            if (static_cast<long long>(line.size()) < x_count)
            {
                cerr << "Row too short at z=" << z << " y=" << y << " (got "
                     << line.size() << ", need " << x_count << ")\n";
                return 1;
            }
            for (long long x = 0; x < x_count; ++x)
            {
                char tag = line[(size_t)x];
                auto it = label_of.find(tag);
                if (it == label_of.end())
                {
                    cerr << "Unknown tag '" << tag << "' at x=" << x
                         << " y=" << y << " z=" << z << "\n";
                    return 1;
                }
                const string &label = it->second;
                // x,y,z position; unit size; label
                cout << x << "," << y << "," << z << ",1,1,1," << label << "\n";
            }
        }
        // a blank line (if present) between slices will be skipped automatically
    }
    return 0;
}