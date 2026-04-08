# I/O Benchmark

## Test

- File size: 8GB
- Platform: Ubuntu 24.04.2 LTS, NVMe SSD

| Method         | Time (s) | Speed (MB/s) |
| -------------- | -------- | ------------ |
| Buffered fread |   12.3   |    650       |
| mmap           |   11.8   |    678       |
| Prefetch thread|   12.0   |    666       |

## Conclusion

mmap is slightly faster and simplest to use.  
**Default: mmap** is used for large file reading in this project.