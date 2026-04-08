# Code is a modified version of chatgpt generated code created by asking it to convert the stream 
# processor from the kickoff lecture to python

from collections import defaultdict

# Read dimensions and positions
bx, by, bz, px, py, pz = input().split(',')
bx = int(bx)
by = int(by)
bz = int(bz)
px = int(px)
py = int(py)
pz = int(pz)

tag_table = {}

# Read symbol-to-label mappings
while True:
    try:
        line = input().strip()
        if not line:
            break
        symbol, label = line.split(',', 1)
        tag_table[symbol] = label
    except EOFError:
        break

# Read grid and output results
for k in range(bz):
    for j in range(by):
        line = input().strip()
        for i in range(bx):
            print(f"{i},{j},{k},1,1,1,{tag_table[line[i]]}")
    _ = input().strip()  # Skip separator line between layers
