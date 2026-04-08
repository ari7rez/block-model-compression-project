fullComp: Code/fullComp/fullComp.cpp
	@g++ Code/fullComp/fullComp.cpp -o bin/fullComp
	@./bin/fullComp < data/input_layered_12x9x3.txt

Lecture_Stream: Code/Lecture_Stream/Lecture_Stream_Processor.cpp
	@g++ Code/Lecture_Stream/Lecture_Stream_Processor.cpp -o bin/Lecture_Stream_Processor
	@./bin/Lecture_Stream_Processor < data/Maptek_Example_Input.txt

passthrough: Code/passthrough/passthrough.cpp
	@g++ Code/passthrough/passthrough.cpp -o bin/passthrough
	@./bin/passthrough < data/input_layered_12x9x3.txt

# Compression, not Compression_optimize
RLE_ZStack: Code/RLE_ZStack/RLE_ZStack_Compression.cpp
	@g++ Code/RLE_ZStack/RLE_ZStack_Compression.cpp -o bin/RLE_ZStack
	@./bin/RLE_ZStack < data/input_layered_12x9x3.txt

Threading_Software: Code/Threading_Software/multthreading.cpp
	@g++ Code/Threading_Software/multthreading.cpp -o bin/Threading_Software
	@./bin/Threading_Software < data/input_layered_12x9x3.txt