kernel: *.cpp
	arm-none-eabi-g++ -r -s -mlong-calls -fno-exceptions -march=armv6-m *.cpp -okernel.bin -O3 -Wall -lgcc
	${EMU_PATH} kernel.bin
