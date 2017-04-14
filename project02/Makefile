serial:
	gcc -Wall -o othello othello.c
	./othello initialbrd.txt evalparams.txt

serial_v3:
	gcc -Wall -o othello othello_v3.c
	./othello initialbrd.txt evalparams.txt

mpi:
	mpicc -DMPI_ENABLED -o othello othello.c
	TMPDIR=/tmp mpirun -np 16 --hostfile my-hostfile ./othello initialbrd.txt evalparams.txt

mpi_v3:
	mpicc -DMPI_ENABLED -o othello othello_v3.c
	TMPDIR=/tmp mpirun -np 16 --hostfile my-hostfile ./othello initialbrd.txt evalparams.txt

valgrind: othello.c othello.h
	mpicc -DMPI_ENABLED -o othello othello.c
	TMPDIR=/tmp mpirun -np 4 --hostfile my-hostfile valgrind ./othello initialbrd.txt evalparams.txt
