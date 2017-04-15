mpi:
	mpicc -DMPI_ENABLED -o othellox othellox.c

sequential:
	gcc -Wall -o othellox othellox.c

valgrind:
	mpicc -DMPI_ENABLED -o othellox othellox.c
	TMPDIR=/tmp mpirun -np 4 --hostfile my-hostfile valgrind ./othellox initialbrd.txt evalparams.txt
