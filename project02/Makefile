serial:
	gcc -Wall -o othello othello.c
	./othello

mpi: othello.c othello.h
	mpicc -DMPI_ENABLED -o othello othello.c
	TMPDIR=/tmp mpirun -np 10 --hostfile my-hostfile ./othello

valgrind: othello.c othello.h
	mpicc -DMPI_ENABLED -o othello othello.c
	TMPDIR=/tmp mpirun -np 4 --hostfile my-hostfile valgrind ./othello
