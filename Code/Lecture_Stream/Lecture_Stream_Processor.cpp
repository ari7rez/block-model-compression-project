#include <iostream>
#include <sstream>

int main() 
{
    using namespace std;
    int bx, by, bz;
    int px, py, pz;
    char comma;
    cin >> bx >> comma >> by >> comma >> bz >> comma >> px >> comma >> py >> comma >> pz >> ws;
    string line;
    char symbol;
    string label;
    unordered_map<char, string> tag_table;
    
    // config and flags
    string blocks = "42M";
    int timeout_mins = 90;
    string configuration = "4CPU/8GB";

    cout << bx << "×" << by << "×" << bz << ", " << blocks << ", " << timeout_mins 
            << "min, " << configuration << endl;
 
    while (true)
    {
        getline(cin, line);
        if (line.empty()) break;
        stringstream line_stream(line);
        line_stream >> symbol >> comma >> label;
        tag_table[symbol] = label;
    }
    for (int k = 0; k < bz; k++)
    {
        for (int j = 0; j < by; j++)
        {
            getline(cin, line);
            for (int i = 0; i < bx; i++)
            {
                cout << i << "," << j << "," << k << ",1,1,1," << tag_table[line[i]] << "\n";
            }
        }
        getline(cin, line);
    }
    return 0;
}