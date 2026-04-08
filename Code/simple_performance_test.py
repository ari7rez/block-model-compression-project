import subprocess
import time
import os
import sys

def simple_test(exe_name, input_file):
    """简单的性能测试"""
    if not os.path.exists(exe_name):
        print(f"可执行文件不存在: {exe_name}")
        return None
        
    if not os.path.exists(input_file):
        print(f"输入文件不存在: {input_file}")
        return None
    
    print(f"测试 {exe_name} 使用 {input_file}")
    
    try:
        # 记录开始时间
        start_time = time.time()
        
        # 运行程序
        with open(input_file, 'r') as f:
            result = subprocess.run(
                [exe_name],
                stdin=f,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=30  # 30秒超时
            )
        
        end_time = time.time()
        execution_time = end_time - start_time
        
        if result.returncode != 0:
            print(f"程序执行失败: {result.stderr}")
            return None
            
        output_size = len(result.stdout.encode('utf-8'))
        
        print(f"  执行时间: {execution_time:.4f}s")
        print(f"  输出大小: {output_size} bytes")
        print(f"  压缩比: {os.path.getsize(input_file) / output_size:.2f}x")
        
        return {
            'time': execution_time,
            'output_size': output_size,
            'compression_ratio': os.path.getsize(input_file) / output_size
        }
        
    except subprocess.TimeoutExpired:
        print(f"程序执行超时")
        return None
    except Exception as e:
        print(f"执行出错: {e}")
        return None

def main():
    """主函数"""
    print("=== 简单性能测试 ===")
    
    # 测试文件
    test_files = [
        ("small", "data/input_small_4x3x2.txt"),
        ("medium", "data/input_medium_8x6x2.txt"),
        ("large", "data/input_layered_12x9x3.txt")
    ]
    
    # 算法列表
    algorithms = [
        "RLE_ZStack_Compression.exe",
        "RLE_ZStack_Compression_optimize.exe", 
        "RLE_ZStack_Compression_optimize_block.exe",  # 新的块级处理优化版本
        "fullComp.exe",
        "passthrough.exe"
    ]
    
    results = {}
    
    for algo in algorithms:
        print(f"\n--- 测试 {algo} ---")
        results[algo] = {}
        
        for test_name, input_file in test_files:
            result = simple_test(algo, input_file)
            if result:
                results[algo][test_name] = result
    
    # 打印总结
    print("\n" + "="*60)
    print("性能测试总结")
    print("="*60)
    
    for algo, algo_results in results.items():
        if not algo_results:
            continue
            
        print(f"\n{algo}:")
        for test_name, result in algo_results.items():
            print(f"  {test_name}: {result['time']:.4f}s, 压缩比 {result['compression_ratio']:.2f}x")

if __name__ == "__main__":
    main()