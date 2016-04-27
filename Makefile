all:
	gcc -o test.o  pthread_pipeline.c -lpthread
clean:
	rm -f test.o
