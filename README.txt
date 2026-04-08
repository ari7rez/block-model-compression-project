# Block Model Compression Project

> **Note:** This document may evolve as the project progresses.

## Overview
This project (Software Engineering Project, Semester 2, 2025) implements a **lossless compressor** for large 3D block models.  
The program reads **uncompressed** block model data from **standard input (stdin)** and writes a **compressed representation** to **standard output (stdout)**.

**Performance is assessed on:**
- **Compression ratio** — smaller output = better  
- **Processing speed** — faster = better  

A leaderboard tracks best submissions. Teams may submit multiple times.

---

## Goals
- Efficiently compress large 3D block models by grouping neighbouring blocks with the same label.  
- Scale to large datasets without exceeding memory limits.  
- Deliver reproducible, maintainable, and well-documented code.  
- Follow consistent Git and documentation standards as a professional team.

---

## Repository Structure

```
.
├─ .vscode/           # Editor settings
├─ Code/              # Source code (compression logic, tests)
├─ Docs/              # Technical design notes & report files
├─ Minutes/           # TA and team meeting minutes
├─ Snapshots/         # Weekly backups or submission states
├─ bin/               # Compiled executables or scripts
├─ data/              # Input datasets and test data
├─ makefile           # Build automation
├─ .gitignore         # Git ignore list
└─ README.md          # This file
```

---

## Input Format

### 1. Header Line
```
x_count, y_count, z_count, parent_x, parent_y, parent_z
```

### 2. Tag Table
Maps single-character tags to descriptive labels:
```
<tag_char>,<label>
```
Ends with a blank line.

### 3. Block Data
- Series of tag characters representing each block.  
- Arranged as:
  - `x_count` per line
  - `y_count` lines per slice
  - `z_count` slices  
- Blank lines separate slices.

---

## Output Format

Your program must output a **compressed representation** that:
- Allows full **lossless reconstruction**  
- Is **deterministic** (same input → same output)  
- Is **self-descriptive** enough for a decoder to rebuild the data  

Output should be written **only to stdout**.

---

## Usage Example

```bash
# Build
make

# Compress (reads stdin, writes stdout)
./bin/compressor < data/sample.txt > compressed.bin

# Optional: Decompress
./bin/decompressor < compressed.bin > recovered.txt

# Check correctness
diff -q data/sample.txt recovered.txt && echo "OK: Lossless"
```

---

## Git & Collaboration Rules

### 🔹 1. Cloning the Repository
```bash
git clone https://github.com/<team>/<repo-name>.git
cd <repo-name>
git checkout main
```
Always clone from **main**, never from a random snapshot branch.

---

### 🔹 2. Branching Rules

- **Never code directly on `main`.**  
- Each task or feature gets its **own branch**:
  ```
  git checkout -b sprintX-feature-shortname
  ```
  Example: `sprint4-fix-parser` or `sprint3-doc-minutes`

- When finished:
  ```
  git add .
  git commit -m "sprint4-fix-parser: summary of change (author: Arian)"
  git push origin sprint4-fix-parser
  ```

---

### 🔹 3. Commit Message Standards

Follow this pattern:
```
sprint<number>-<task>: <short summary> (author: <firstName>)
```

**Examples:**
```
sprint4-compression: improve block grouping (author: Mohammad)
sprint4-docs: add Week 8 minutes (author: Arian)
sprint3-testing: fix I/O buffer issue (author: Calvin)
```

✅ **Tips:**
- Use **lowercase** task names (no spaces).  
- Keep message under **80 characters**.  
- Always include **author** at the end.  
- For big changes, include details in the pull request description.

---

### 🔹 4. Merging Rules

1. **Open a Pull Request (PR)** into `main`.  
2. Tag at least **one reviewer** (TA or teammate).  
3. Resolve conflicts locally before merging:
   ```bash
   git fetch origin
   git merge origin/main
   ```
4. Only merge when:
   - Code **builds successfully**
   - **All conflicts resolved**
   - Commit messages follow the **standard**
5. Use **Squash and Merge** on GitHub to keep the main branch clean.  
6. Delete merged feature branches after review approval.

---

### 🔹 5. Snapshot & Minutes Policy

- Save weekly progress snapshots under `/Snapshots/`  
  Format: `snapshot-week_<number>`  
- Upload all TA and internal meeting minutes to `/Minutes/`  
  Example: `TA Minutes SEP Week 8 Sem 2.docx`

---

## Development & Performance Guidelines

- Use **efficient memory access** (slice-based or streamed).  
- Use **buffered I/O** (avoid per-character reads).  
- Maintain **clear comments** and error handling.  
- Benchmark your code:
  ```
  time ./bin/compressor < data/input.txt > output.bin
  ```

---

## Licensing

This project is intended for **educational and academic purposes only** as part of the University coursework.  
All contributors retain authorship of their work. Redistribution or commercial use is not permitted without team consent.  
For any reuse outside the course context, please provide attribution to the original team.
