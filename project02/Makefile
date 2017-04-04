serial:
	gcc -Wall -o othello othello.c
	./othello

mpi: othello.c othello.h
	mpicc -DMPI_ENABLED -o othello othello.c
	mpirun -np 10 --hostfile my-hostfile ./othello

