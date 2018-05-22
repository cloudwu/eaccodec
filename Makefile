test.exe : eaccodec.c test.c
	gcc -Wall -O2 -o $@ $^

clean :
	rm test.exe
