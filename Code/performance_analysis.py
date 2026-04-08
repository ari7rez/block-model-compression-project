import subprocess
import time
import psutil
import os
import sys

def measure_performance(exe_path, input_file, iterations=3):
    """测量程序性能，包括时间和内存使用"""
    times = []
    memory_usage = []
    
    for i in range(iterations):
        print(f"运行第 {i+1} 次测试...")
        
        # 启动进程
        with open(input_file, 'r') as f:
            process = subprocess.Popen(
                [exe_path],
                stdin=f,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
        
        # 监控内存使用
        start_time = time.time()
        max_memory = 0
        
        while process.poll() is None:
            try:
                ps_process = psutil.Process(process.pid)
                memory_info = ps_process.memory_info()
                max_memory = max(max_memory, memory_info.rss / 1024 / 1024)  # MB
                time.sleep(0.01)  # 10ms采样间隔
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                break
        
        end_time = time.time()
        execution_time = end_time - start_time
        
        times.append(execution_time)
        memory_usage.append(max_memory)
        
        # 获取输出大小 - 只有在进程还在运行时才调用communicate
        if process.poll() is None:
            stdout, stderr = process.communicate()
        else:
            stdout, stderr = process.communicate()
        output_size = len(stdout.encode('utf-8'))
        
        if stderr:
            print(f"错误: {stderr}")
    
    return {
        'avg_time': sum(times) / len(times),
        'min_time': min(times),
        'max_time': max(times),
        'avg_memory': sum(memory_usage) / len(memory_usage),
        'max_memory': max(memory_usage),
        'output_size': output_size
    }

def analyze_compression_algorithms():
    """分析所有压缩算法的性能"""
    algorithms = [
        ("RLE_ZStack_Compression", "RLE_ZStack_Compression.exe"),
        ("RLE_ZStack_Compression_optimize", "RLE_ZStack_Compression_optimize.exe"),
        ("fullComp", "fullComp.exe"),
        ("passthrough", "passthrough.exe")
    ]
    
    test_files = [
        ("small", "data/input_small_4x3x2.txt"),
        ("medium", "data/input_medium_8x6x2.txt"),
        ("large", "data/input_layered_12x9x3.txt")
    ]
    
    results = {}
    
    for algo_name, exe_name in algorithms:
        print(f"\n=== 测试 {algo_name} ===")
        
        # 检查exe文件是否存在
        if not os.path.exists(exe_name):
            print(f"可执行文件不存在: {exe_name}")
            continue
            
        results[algo_name] = {}
        
        for test_name, input_file in test_files:
            if not os.path.exists(input_file):
                print(f"测试文件不存在: {input_file}")
                continue
                
            print(f"\n测试 {test_name} 数据集...")
            perf = measure_performance(exe_name, input_file)
            results[algo_name][test_name] = perf
            
            print(f"  平均时间: {perf['avg_time']:.4f}s")
            print(f"  最大内存: {perf['max_memory']:.2f}MB")
            print(f"  输出大小: {perf['output_size']} bytes")
    
    return results

def print_performance_report(results):
    """打印性能报告"""
    print("\n" + "="*80)
    print("性能分析报告")
    print("="*80)
    
    for algo_name, algo_results in results.items():
        print(f"\n{algo_name}:")
        print("-" * 50)
        
        for test_name, perf in algo_results.items():
            print(f"  {test_name}:")
            print(f"    时间: {perf['avg_time']:.4f}s (范围: {perf['min_time']:.4f}-{perf['max_time']:.4f}s)")
            print(f"    内存: {perf['max_memory']:.2f}MB")
            print(f"    输出: {perf['output_size']} bytes")

if __name__ == "__main__":
    print("开始性能分析...")
    results = analyze_compression_algorithms()
    print_performance_report(results)
