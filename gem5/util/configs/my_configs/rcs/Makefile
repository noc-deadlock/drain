all: testing_cpp testing_c

testing_cpp: testing_cpp.o
	g++ -o testing_cpp testing_cpp.o ../../../util/m5/m5op_x86.o ../../../util/m5/m5_mmap.o

testing_c: testing_c.o
	gcc -o testing_c testing_c.o ../../../util/m5/m5op_x86.o ../../../util/m5/m5_mmap.o

testing_cpp.o: testing.cpp
	g++ -O2 -DM5OP_ADDR=0xFFFF0000 -I../../../include -o testing_cpp.o -c testing.cpp

testing_c.o: testing.c
	gcc -O2 -DM5OP_ADDR=0xFFFF0000 -I../../../include -o testing_c.o -c testing.c

clean:
	rm *.o testing_c testing_cpp
