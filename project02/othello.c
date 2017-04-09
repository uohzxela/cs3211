#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "othello.h"

#ifdef MPI_ENABLED
#include "mpi.h"
MPI_Status status;
#endif

#define min(a,b) (a < b ? a : b)

#define WIDTH 10
#define HEIGHT 10

#define ROWS HEIGHT-2
#define COLS WIDTH-2
#define SQUARES WIDTH*HEIGHT

#define UP -WIDTH
#define DOWN WIDTH
#define LEFT -1
#define RIGHT 1
#define UP_RIGHT -(WIDTH-1)
#define DOWN_RIGHT WIDTH+1
#define DOWN_LEFT WIDTH-1
#define UP_LEFT -(WIDTH+1)

int DIRECTIONS[] = {UP, UP_RIGHT, RIGHT, DOWN_RIGHT, DOWN, DOWN_LEFT, LEFT, UP_LEFT};

char *INITIAL_WHITE_POS[] = {"b3", "b5", "b6", "c4", "c5", "d4", "d5", "d6","d7","e2","e3","e4","e6","f3","g3","g4","g6","h4","h5"};;
char *INITIAL_BLACK_POS[] = {"a4", "a5", "a6", "b4", "c1","c3", "d1", "d2", "d3","e5", "e7", "e8","f2","f4","f5","g5","h3","h6"};

char EMPTY = '.';
char BLACK = '@';
char WHITE = 'o';
char OUTER = '?';

int myid, slaves;
#define MASTER_ID slaves

#define SUBPROBLEM_TAG 1
#define RETURN_TAG 2
#define REQUEST_TAG 3
#define CUTOFF_TAG 4
#define CANCELLATION_TAG 5
#define TERMINATION_TAG 6

long long comm_time = 0;
long long comp_time = 0;

// rename to Result
struct _Tuple {
    int move;
    int score;
};

struct _Job {
    char board[SQUARES];
    int alpha;
    int beta;
    char player;
    int depth;
    bool finished;
};

/** 
 * Determines the current time
 *
 **/
long long wall_clock_time()
{
#ifdef LINUX
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (long long)(tp.tv_nsec + (long long)tp.tv_sec * 1000000000ll);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)(tv.tv_usec * 1000 + (long long)tv.tv_sec * 1000000000ll);
#endif
}

char* index2label(int index)
{
    int row = index/WIDTH;
    char col = 'a' + (index%WIDTH) - 1;
    char *label = malloc(sizeof(char) * 4);
    snprintf(label, sizeof(label), "%c%d", col, row);
    return label;
}

int label2index(const char *label)
{
    int col = label[0] - 'a' + 1;
    int row = atoi(label+1);
    return row*WIDTH+col;
}

char* create_board()
{
    char *board = malloc(sizeof(char)*(SQUARES));
    size_t i;
    for (i=0; i<SQUARES; i++) {
        if (i >= WIDTH+1 &&
                i < WIDTH*(HEIGHT-1)-1 &&
                1 <= i % WIDTH &&
                i % WIDTH <= COLS) {
            board[i] = EMPTY;
        } else {
            board[i] = OUTER;
        }
    }
    for (i=0; i<sizeof(INITIAL_WHITE_POS)/sizeof(INITIAL_WHITE_POS[0]); i++) {
        board[label2index(INITIAL_WHITE_POS[i])] = WHITE;
    }
    for (i=0; i<sizeof(INITIAL_BLACK_POS)/sizeof(INITIAL_BLACK_POS[0]); i++) {
        board[label2index(INITIAL_BLACK_POS[i])] = BLACK;
    }
    return board;
}

void print_board(char *board)
{
    int i;
    printf("\n");
    printf("     ");
    for (i=0; i<COLS; i++) { printf("%c ", 'a'+i); }
    for (i=0; i<SQUARES; i++) {
        char c = board[i];
        if (c == OUTER) c = ' ';
        if (i % WIDTH == 0) {
            printf("\n");
            printf("%d %c", i/WIDTH, (i/WIDTH > 9) ? '\0' : ' ');
        }
        printf("%c ", c);
        if ((i+1)%WIDTH == 0) {
            printf("%d %c", i/WIDTH, (i/WIDTH > 9) ? '\0' : ' ');
        }
    }
    printf("\n");
    printf("     ");
    for (i=0; i<COLS; i++) { printf("%c ", 'a'+i); }
    printf("\n");
}

char* copy_board(char *src)
{
    char *copy = malloc(sizeof(char)*SQUARES);
    memcpy(copy, src, sizeof(char)*SQUARES);
    return copy;
}

int evaluate(char player, char *board)
{
    int mine = 0, theirs = 0;
    char opp = opponent(player);
    size_t i;
    for (i=0; i<SQUARES; i++) {
        if (board[i] == player) {
            mine += 1;
        } else if (board[i] == opp) {
            theirs += 1;
        }
    }
    // printf("mine-theirs: %d\n", mine-theirs);
    return mine - theirs;
}

bool is_legal(int move, char player, char *board)
{
    if (!is_valid(move) || board[move] != EMPTY) return false;
    bool has_bracket = false;
    size_t i;
    // printf("sizeof(DIRECTIONS)/sizeof(DIRECTIONS[0] = %d\n", sizeof(DIRECTIONS)/sizeof(DIRECTIONS[0]));
    for (i=0; i<sizeof(DIRECTIONS)/sizeof(DIRECTIONS[0]); i++) {
        int d = DIRECTIONS[i];
        if (find_bracket(move, player, board, d) > -1) {
            return true;
        }
    }
    return false;
}

bool is_valid(int move)
{
    return 0 <= move && move < SQUARES;
}

char opponent(char player)
{
    return player == WHITE ? BLACK : WHITE;
}

int get_move(char player, char *board, int depth)
{
    char *copy = copy_board(board);
    int move = alphabeta_strategy(player, copy, depth);
    printf("Move: %s\n", index2label(move));
    if (!is_valid(move) || !is_legal(move, player, board)) {
        fprintf(stderr, "%s cannot move to square %d\n", (player == WHITE) ? "White" : "Black", move);
        exit(1);
    }
    return move;
}

int find_bracket(int square, char player, char *board, int direction)
{
    int bracket = square + direction;
    if (!is_valid(bracket) || board[bracket] == player) return -1;

    char opp = opponent(player);
    while (board[bracket] == opp) {
        bracket += direction;
    }
    return (board[bracket] == OUTER || board[bracket] == EMPTY) ? -1 : bracket;
}

void make_flips(int move, char player, char *board, int direction)
{
    int bracket = find_bracket(move, player, board, direction);
    if (bracket < 0) return;

    int square = move + direction;
    while (square != bracket) {
        board[square] = player;
        square += direction;
    }
}

char* make_move(int move, char player, char *board)
{
    board[move] = player;
    size_t i;
    //printf("sizeof(DIRECTIONS) = %d\n", sizeof(DIRECTIONS));
    //printf("sizeof(DIRECTIONS[0]) = %d\n", sizeof(DIRECTIONS[0]));
    for (i=0; i<sizeof(DIRECTIONS)/sizeof(DIRECTIONS[0]); i++) {
        int d = DIRECTIONS[i];
        make_flips(move, player, board, d);
    }
    return board;
}

int final_value(char player, char *board)
{
    int diff = evaluate(player, board);
    if (diff < 0) {
        return INT_MIN;
    } else if (diff > 0) {
        return INT_MAX;
    }
    return diff;
}

bool any_legal_move(char player, char *board)
{
    int move;
    for (move=0; move<SQUARES; move++) {
        if (is_legal(move, player, board)) {
            return true;
        }
    }
    return false;
}

Tuple* tuple(int score, int move)
{
    Tuple *ret = malloc(sizeof(Tuple));
    ret->score = score;
    ret->move = move;
    return ret;
}

int* generate_moves(char player, char *board, int *n)
{
    int *moves = malloc(sizeof(int) * SQUARES);
    int move;
    *n = 0;

    for (move=0; move<SQUARES; move++) {
        if (is_legal(move, player, board)) {
            moves[(*n)] = move;
            (*n)++;
        }
    }
    return (*n) > 0 ? moves : NULL;
}

int alphabeta_strategy(char player, char *board, int depth)
{
    return alphabeta(player, board, depth, -9999, 9999)->move;
}

Tuple* alphabeta(char player, char *board, int depth, int alpha, int beta)
{
    if (depth == 0) {
        // TODO: change score() to evaluate()
        return tuple(evaluate(player, board), -1);
    }
    int n;
    int *moves = generate_moves(player, board, &n);
    // no legal moves for current player
    if (moves == NULL) {
        // opponent also doesn't have any legal moves
        if (!any_legal_move(opponent(player), board)) {
            return tuple(final_value(player, board), -1);
        }
        // continue to recurse down without making any moves for current player
        Tuple *ret = alphabeta(opponent(player), board, depth-1, -beta, -alpha);
        return tuple(-(ret->score), -1);
    }

    int best_move = -1;
    int score, i;

    for (i=0; i<n; i++) {
        if (alpha >= beta) break;
        Tuple *ret = alphabeta(opponent(player),
                make_move(moves[i], player, copy_board(board)),
                depth-1,
                -beta,
                -alpha);
        score = -(ret->score);
        if (score > alpha) {
            alpha = score;
            best_move = moves[i];
        }
    }
    return tuple(alpha, best_move);
}

void play_serial(char player, int depth)
{
    char *board = create_board();
    printf("Before score: %d", evaluate(player, board));
    print_board(board);
    int move = get_move(player, board, depth);

    make_move(move, player, board);
    printf("After score: %d", evaluate(player, board));
    print_board(board);
}

int random_slave() {
	int slave;
	do {
		slave = rand() % (slaves+1);
	} while (slave == myid);
	return slave;
}

#ifdef MPI_ENABLED
int master(char player, char *board, int depth)
{

    Job *job = malloc(sizeof(Job));
    Tuple *result = malloc(sizeof(Tuple));

    job->depth = depth;
    job->alpha = -9999;
    job->beta = 9999;
    memcpy(job->board, create_board(), sizeof(char)*SQUARES);
    MPI_Send(job, sizeof(Job), MPI_BYTE, 0, SUBPROBLEM_TAG, MPI_COMM_WORLD);
    printf(" --- MASTER: sending first job to SLAVE 0\n");
    MPI_Recv(result, sizeof(Tuple), MPI_BYTE, 0, RETURN_TAG, MPI_COMM_WORLD, &status);
   	printf(" --- MASTER: received final result\n");
   	int i;
   	for (i=0; i<slaves; i++) {
   		MPI_Send(&myid, 1, MPI_INT, i, TERMINATION_TAG, MPI_COMM_WORLD);
   	}
   	// exit(1);
    return result->move;
}

void send_cutoff(int *slave_list)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i]) {
			MPI_Send(&myid, 1, MPI_INT, i, CUTOFF_TAG, MPI_COMM_WORLD);
			slave_list[i] = false;
		}
	}
}

void send_cancellation(int slave)
{
	MPI_Send(&myid, 1, MPI_INT, slave, CANCELLATION_TAG, MPI_COMM_WORLD);
}

void send_random_request() 
{
	int slave = random_slave();
	MPI_Send(&myid, 1, MPI_INT, slave, REQUEST_TAG, MPI_COMM_WORLD);
	// if (myid==0) printf(" --- SLAVE %d: sent a request to %d\n", myid, slave);
}

void send_return(int master, Tuple *result)
{
	MPI_Send(result, sizeof(Tuple), MPI_BYTE, master, RETURN_TAG, MPI_COMM_WORLD);
}

bool is_slave_list_empty(int *slave_list)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i]) return false;
	}
	return true;
}

void print_slave_list(int *slave_list)
{
	int i;
	printf(" --- SLAVE %d: ", myid);
	for (i=0; i<slaves; i++) {
		printf("%d:%s ", i, slave_list[i] ? "T" : "F");
	}
	printf("\n");
}
void slave()
{
	int score, master, slave, n_moves, i, from, best_move;
	bool idle = true, has_message = false;
	long long before, after;
	int alpha, beta, depth;
	char player, *board;

	int *move_list;
	int *slave_list = malloc(sizeof(int) * slaves);
	for (i = 0; i<slaves; i++) slave_list[i] = false;
	Job *job = malloc(sizeof(Job));
	Tuple *result = malloc(sizeof(Tuple));
	int dirty = false;
	// TODO: enforce evaluation limit
    while (true) {
    	before = wall_clock_time();
    	while (idle) {
    		if (myid != 0 || dirty) {
	    		send_random_request();
	    		dirty = true;
    		}
    		// if (myid == 0)
    			MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    		// printf(" --- SLAVE %d: probing message queue\n", myid);
    		// probe_message_queue(&has_message);
 			// if (has_message) {
	    		// receive subproblem
	    		if (status.MPI_TAG == SUBPROBLEM_TAG) {
	    			MPI_Recv(job, sizeof(Job), MPI_BYTE, MPI_ANY_SOURCE, SUBPROBLEM_TAG, MPI_COMM_WORLD, &status);
	    			printf(" --- SLAVE %d: received a subproblem from %d\n", myid, status.MPI_SOURCE);
	    			idle = false;
	    			master = status.MPI_SOURCE;
	    			break;
	    			// initialize subproblem
	    		} else if (status.MPI_TAG == CANCELLATION_TAG) {

	    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CANCELLATION_TAG, MPI_COMM_WORLD, &status);
	    			// if (myid==0)printf(" --- SLAVE %d: received a cancellation from %d\n", myid, status.MPI_SOURCE);
	    			// delay a bit?
	    			// request randomly again
		    		// MPI_Send(&myid, 1, MPI_INT, random_slave(), REQUEST_TAG, MPI_COMM_WORLD);
		    		// send_random_request();
	    		} else if (status.MPI_TAG == REQUEST_TAG) {
	    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_TAG, MPI_COMM_WORLD, &status);
	    			// MPI_Send(&myid, 1, MPI_INT, status.MPI_SOURCE, CANCELLATION_TAG, MPI_COMM_WORLD);
	    			send_cancellation(status.MPI_SOURCE);

	    		} else if (status.MPI_TAG == TERMINATION_TAG) {
	    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &status);
	    			after = wall_clock_time();
	    			comm_time += after - before;
	    			fprintf(stderr, " --- SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0);
	    			return;
	    		} else {
	    			// drop message without processing
	    			MPI_Recv(job, sizeof(Job), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	    		}
 			// } 

 			// if (!has_message || status.MPI_TAG != SUBPROBLEM_TAG){
	    		// request randomly
	    		// send_random_request();
	    		// MPI_Send(&myid, 1, MPI_INT, random_slave(), REQUEST_TAG, MPI_COMM_WORLD);
 			// }

    	}
    	after = wall_clock_time();
    	comm_time += after - before;

    	player = job->player;
    	board = job->board;
    	alpha = job->alpha;
    	beta = job->beta;
    	depth = job->depth;
    	move_list = generate_moves(player, board, &n_moves);
    	// i = 0;
    	// while (i<n_moves || !is_slave_list_empty(slave_list)) {
    	for (i=0; i<n_moves; i++) {
    		if (alpha >= beta) {
    			send_cutoff(slave_list);
    			break;
    		}
    		before = wall_clock_time();
    		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &has_message, &status);
    		if (has_message) {
				if (status.MPI_TAG == REQUEST_TAG) {

					MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_TAG, MPI_COMM_WORLD, &status);
					printf(" --- SLAVE %d: received a request from %d\n", myid, status.MPI_SOURCE);
					slave = status.MPI_SOURCE;
					if (i == 0 || slave_list[slave]) {
						send_cancellation(slave);
						printf(" --- SLAVE %d: sent a cancellation to %d\n", myid, status.MPI_SOURCE);
						// MPI_Send(&myid, 1, MPI_INT, slave, CANCELLATION_TAG, MPI_COMM_WORLD);
					} else if (i < n_moves) {
						slave_list[slave] = true;
						// TODO: send subproblem(i) to requester
						memcpy(job->board, make_move(move_list[i], player, copy_board(board)), sizeof(char)*SQUARES);
						job->player = opponent(player);
						job->depth = depth-1;
						job->alpha = -beta;
						job->beta = -alpha;
						MPI_Send(job, sizeof(Job), MPI_BYTE, slave, SUBPROBLEM_TAG, MPI_COMM_WORLD);
						printf(" --- SLAVE %d: sent a subproblem to %d\n", myid, slave);
						continue; // don't evaluate current move
					}
				}
				if (status.MPI_TAG == RETURN_TAG) {
					// react_upon_return()
					MPI_Recv(result, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
					printf(" --- SLAVE %d: received a result from %d\n", myid, status.MPI_SOURCE);
					slave_list[status.MPI_SOURCE] = false;
					// TODO: 
					if (result->score > alpha) {
						alpha = result->score;
						best_move = result->move;
					}
				}
				if (status.MPI_TAG == CUTOFF_TAG) {
					MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &status);
					printf(" --- SLAVE %d: received a cutoff from %d\n", myid, status.MPI_SOURCE);
					if (status.MPI_SOURCE != master) {
						printf(" --- SLAVE %d: false cutoff\n", myid);
						continue;
					}
					// assert from == master
					send_cutoff(slave_list);
					break;
				}
    		}
    		after = wall_clock_time();
    		comm_time += after - before;
    		// i++;
    		// if (i >= n_moves) continue;
    		printf(" --- SLAVE %d: one iteration of alphabeta\n", myid);
    		// TODO: evaluate current move
    		before = wall_clock_time();
	        Tuple *ret = alphabeta(opponent(player),
	                make_move(move_list[i], player, copy_board(board)),
	                depth-1,
	                -beta,
	                -alpha);
	        score = -(ret->score);
	        if (score > alpha) {
	            alpha = score;
	            best_move = move_list[i];
	        }
	        after = wall_clock_time();
	        comp_time += after - before;
    	}
    	// subproblem finished
    	// TODO: send RETURN to master
    	while (!is_slave_list_empty(slave_list)) {
    		printf(" --- SLAVE %d: waiting for result\n", myid);
    		print_slave_list(slave_list);
			MPI_Recv(result, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
			printf(" --- SLAVE %d: received a result from %d\n", myid, status.MPI_SOURCE);
			slave_list[status.MPI_SOURCE] = false;
			// TODO: 
			if (result->score > alpha) {
				alpha = result->score;
				best_move = result->move;
			}
    	}
    	result->score = alpha;
    	result->move = best_move;
    	MPI_Send(result, sizeof(Tuple), MPI_BYTE, master, RETURN_TAG, MPI_COMM_WORLD);
    	printf(" --- SLAVE %d: sent a result to %d\n", myid, master);
    	// send_return(master, tuple(alpha, best_move));
    	idle = true;
    }

    fprintf(stderr, " --- SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0);
}
void play(char player, int depth)
{
	int best_move;
	char *board = create_board();

	if (myid == MASTER_ID) {
	    printf("Before score: %d", evaluate(player, board));
	    print_board(board);
		best_move = master(player, board, depth);
	} else {
		slave();
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if (myid == MASTER_ID) {
	    printf("move: %s\n", index2label(best_move));
	    make_move(best_move, player, board);

	    printf("After score: %d", evaluate(player, board));
	    print_board(board);
	}
}
#endif

int main(int argc, char *argv[])
{
	long long before, after;
	srand(time(0));
#ifdef MPI_ENABLED
	int nprocs;
	before = wall_clock_time();
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    slaves = nprocs - 1;

    play(WHITE, 10);
    after = wall_clock_time();
    if (myid == MASTER_ID) {
    	fprintf(stderr, " --- PARALLEL: total_elapsed_time=%6.2f seconds\n", (after - before) / 1000000000.0);
    }
    MPI_Finalize();

#else
    before = wall_clock_time();
    play_serial(WHITE, 10);
    after = wall_clock_time();
    fprintf(stderr, " --- SERIAL: total_elapsed_time=%6.2f seconds\n", (after - before) / 1000000000.0);
#endif
    return 0;
}