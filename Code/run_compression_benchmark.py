import subprocess
import time

def compile(src, out):
    subprocess.run(["g++", "-O2", "-std=c++17", src, "-o", out], check=True)

def run_and_time(exe, input_file, output_file):
    start = time.time()
    with open(input_file, "r") as fin, open(output_file, "w") as fout:
        subprocess.run([f"./{exe}"], stdin=fin, stdout=fout, check=True)
    end = time.time()
    return end - start

if __name__ == "__main__":
    compile("Code/RLE_ZStack/RLE_ZStack_Compression.cpp", "compression")
    compile("Code/RLE_ZStack/RLE_ZStack_Compression_optimize.cpp", "compression_optimize")

    exe_list = ["compression", "compression_optimize"]
    input_list = [
        "data/input_small_4x3x2.txt",
        "data/input_medium_8x6x2.txt",
        "data/input_layered_12x9x3.txt"
    ]
    results = {exe: {} for exe in exe_list}

    for exe in exe_list:
        for input_file in input_list:
            output_file = f"data/{exe}_{input_file.split('/')[-1]}_output.txt"
            t = run_and_time(exe, input_file, output_file)
            results[exe][input_file] = f"{t:.4f} sec"

    print("\ntime consume table:")
    header = "{:<30}".format("code file") + "".join(["{:>25}".format(input_file) for input_file in input_list])
    print(header)
    print("-" * (30 + 25 * len(input_list)))
    for exe in exe_list:
        row = "{:<30}".format(exe)
        for input_file in input_list:
            row += "{:>25}".format(results[exe][input_file])
        print(row)