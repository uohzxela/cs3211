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

int myid, slaves, nprocs;
long max_boards = 10000000, boards_evaluated = 0;
int max_depth;

#define MAX_BOARDS max_boards
#define MAX_DEPTH max_depth
#define MAX_BOARDS_PER_SLAVE max_boards/slaves

#define MASTER_ID 0

#define SUBPROBLEM_TAG 1
#define RETURN_TAG 2
#define REQUEST_TAG 3
#define CUTOFF_TAG 4
#define CANCELLATION_TAG 5
#define TERMINATION_TAG 6
#define SHARE_TAG 7
#define FIND_INACTIVE_TAG 8
#define SET_INACTIVE_TAG 9
#define STATUS_TAG 10
#define CUTOFF_ACK_TAG 11

#define MAX_CHILDREN 4

long long comm_time = 0;
long long comp_time = 0;
long long before, after;

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
    int move;
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
void send_cutoff(int *children_list, int n_children)
{
	int i, from;
	for (i=0; i<n_children; i++) {
		MPI_Send(&myid, 1, MPI_INT, children_list[i], CUTOFF_TAG, MPI_COMM_WORLD);
		printf(" --- SLAVE %d: cutting off child %d\n", myid, children_list[i]);
		MPI_Recv(&from, 1, MPI_INT, children_list[i], CUTOFF_ACK_TAG, MPI_COMM_WORLD, &status);
		printf(" --- SLAVE %d: successfully cut off child %d\n", myid, children_list[i]);
	}

}

void send_cancellation(int slave)
{
	MPI_Send(&myid, 1, MPI_INT, slave, CANCELLATION_TAG, MPI_COMM_WORLD);
	// printf(" --- %s %d: sent a cancellation to %d\n", myid == MASTER_ID ? "MASTER" : "SLAVE", myid, slave);
}

void send_random_request() 
{
	int slave = random_slave();
	MPI_Send(&myid, 1, MPI_INT, slave, REQUEST_TAG, MPI_COMM_WORLD);
	// printf(" --- SLAVE %d: sent a request to %d\n", myid, slave);
}

void send_request_to_master()
{
	MPI_Send(&myid, 1, MPI_INT, MASTER_ID, REQUEST_TAG, MPI_COMM_WORLD);
	printf(" --- SLAVE %d: sent a request to master\n", myid);
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
	printf(" --- %s %d: ", myid == MASTER_ID ? "MASTER" : "SLAVE", myid);
	for (i=0; i<slaves; i++) {
		printf("%d:%s ", i, slave_list[i] ? "T" : "F");
	}
	printf("\n");
}
bool has_active_slaves(int *slave_list)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i]) return true;
	}
	return false;
}
int* create_children_list(int *n_children)
{	
	*n_children = 0;
	int i;
    int *children_list = malloc(sizeof(int) * MAX_CHILDREN);
    for (i=0; i<MAX_CHILDREN; i++) {
    	if ((myid * MAX_CHILDREN + 1 + i) < nprocs) {
    		children_list[(*n_children)++] = myid * MAX_CHILDREN + 1 + i;
    	} 
    }
    return children_list;
}
bool has_active_children(int *active_list, int n_children)
{
	int i;
	for (i=0; i<n_children; i++) {
		if (active_list[i]) return true;
	}
	return false;
}
int master(char player, char *board, int depth)
{
    int n_moves, i, from, idle, n_children = 0;
    int *move_list = generate_moves(player, board, &n_moves);
    printf("n_moves (%d): [", n_moves);
    for (i=0;i<n_moves;i++) {
        printf("%s ", index2label(move_list[i]));
    }
    printf("]\n");
    int *children_list = create_children_list(&n_children);
    printf(" --- MASTER: children list [");
    for (i=0; i<n_children; i++) {
    	printf("%d ", children_list[i]);
    }
    printf("]\n");
    int alpha = -9999, beta = 9999;
    int best_move = move_list[0];
    // Tuple *result = alphabeta(opponent(player),
    //         make_move(move_list[0], player, copy_board(board)),
    //         depth-1,
    //         -beta,
    //         -alpha);
    // alpha = -(result->score);


    Job *job = malloc(sizeof(Job));
	Tuple *result = malloc(sizeof(Tuple));

    for (i=0; i<min(n_moves, n_children); i++) {

    	int child = children_list[i];
        job->player = opponent(player);
        memcpy(job->board, make_move(move_list[i], player, copy_board(board)), sizeof(char)*SQUARES);
        job->depth = depth-1;
        job->alpha = -beta;
        job->beta = -alpha;
        job->move = move_list[i];
        
        MPI_Send(job, sizeof(Job), MPI_BYTE, child, SUBPROBLEM_TAG, MPI_COMM_WORLD);
        printf(" --- MASTER: sent subproblem to %i\n", child);
        // slave_list[slave] = true;
        // MPI_Recv(&idle, 1, MPI_INT, slave, STATUS_TAG, MPI_COMM_WORLD, &status);
    }
    // if num of children < num of moves
    for (i=n_children; i<n_moves; i++) {
		if (alpha >= beta) {
			// TODO: send CUTOFF to children
			printf(" --- SLAVE %d: CUTOFF\n", myid);
			send_cutoff(children_list, n_children);
			// is_cutoff = true;
			break;
		}
        MPI_Recv(result, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
        printf(" --- MASTER: received result from %i\n", status.MPI_SOURCE);

        if (-result->score >= alpha) {
            best_move = result->move;
            alpha = -result->score;
        }

        memcpy(job->board, make_move(move_list[i], player, copy_board(board)), sizeof(char)*SQUARES);
        job->player = opponent(player);
        job->depth = depth-1;
        job->alpha = -beta;
        job->beta = -alpha;
        job->move = move_list[i];
        
        MPI_Send(job, sizeof(Job), MPI_BYTE, status.MPI_SOURCE, SUBPROBLEM_TAG, MPI_COMM_WORLD);
        printf(" --- MASTER: sent subproblem(2) to %i\n", status.MPI_SOURCE);
        // MPI_Recv(&idle, 1, MPI_INT, status.MPI_SOURCE, STATUS_TAG, MPI_COMM_WORLD, &status);
        // current_move[status.MPI_SOURCE] = moves[i];
    }
    for(i = 0; i < min(n_moves, n_children); i++) {
        MPI_Recv(result, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
        if(-result->score >= alpha) {
            alpha = -result->score;
            best_move = result->move;
            // slave_list[status.MPI_SOURCE] = false;
            // printf("best move updated: %d\n", best_move);
        }
        printf("--- MASTER: received result from %i, alpha = %d\n", status.MPI_SOURCE, alpha);
    }
   	printf(" --- MASTER: received final result\n");

   	// int from;
   	// while (!is_slave_list_empty(slave_list)) {
   		// print_slave_list(slave_list);
	   	for (i=1; i<nprocs; i++) {

	   		// if (!slave_list[i]) continue;
	   		// printf("--- MASTER: sending termination to %i\n", i);
	   		MPI_Send(&myid, 1, MPI_INT, i, TERMINATION_TAG, MPI_COMM_WORLD);
	   		MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &status);
	   		// printf("--- MASTER: terminated termination to %i\n", i);
	   		// slave_list[status.MPI_SOURCE] = false;
	   	}
   	// }

   	// exit(1);
    return best_move;
}

Tuple* jamboree(Job job)
{
	char player = job.player;
	char *board = job.board;
	int alpha = job.alpha;
	int beta = job.beta;
	int depth = job.depth;
	int parent_move = job.move;
	int n_moves, score, has_message;
	char *copy;

	if (depth <= 0) {
		return tuple(evaluate(player, board), parent_move);
	}

	before = wall_clock_time();
	int *move_list = generate_moves(player, board, &n_moves);
	printf(" --- SLAVE %d: %d moves\n", myid, n_moves);

	if (!n_moves) {
		return tuple(evaluate(player, board), parent_move);
	} else {
		printf(" --- SLAVE %d: one iteration of alphabeta\n", myid);
		copy = make_move(move_list[0], player, copy_board(board));
	    score = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
	}
	after = wall_clock_time();
	comp_time += after - before;

	if (score > alpha) {
		alpha = score;
	}
	if (alpha >= beta) {
		return tuple(alpha, parent_move);
	}

	int n_children, child;
	int *children_list = create_children_list(&n_children);
	print_children_list(children_list, n_children);
	int *active_list = create_active_list(slaves);

	Tuple res;

	before = wall_clock_time();
    for (i=0; i<min(n_children, n_moves-1); i++) {
    	int move_index = i+1;
    	int child = children_list[i];
    	update_job(&job, move_list[move_index], player, board, depth, alpha, beta);
        send_subproblem(child, &job);
        active_list[child_order(child)] = true;
    }

    comm_time += wall_clock_time() - before;

	for (i=n_children; i<n_moves; i++) {
		if (alpha >= beta) {
			// TODO: send CUTOFF to children
			printf(" --- SLAVE %d: CUTOFF\n", myid);
			send_cutoff(children_list, n_children);
			// is_cutoff = true;
			break;
		}
		before = wall_clock_time();
		// printf("probing\n");

		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &has_message, &status);

		if (has_message) {
			if (status.MPI_TAG == RETURN_TAG) {

				MPI_Recv(&res, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
				int child = status.MPI_SOURCE;
				printf(" --- SLAVE %d: received a result from %d\n", myid, child);
				// slave_list[status.MPI_SOURCE] = false;
				// active_list[(child % (myid * MAX_CHILDREN)) - 1] = false;

				if (-res.score > alpha) {
					alpha = -res.score;
				}
		        job->player = opponent(player);
		        memcpy(job->board, make_move(move_list[i], player, copy_board(board)), sizeof(char)*SQUARES);
		        job->depth = depth-1;
		        job->alpha = -beta;
		        job->beta = -alpha;
// 
		        printf(" --- SLAVE %d: sending subproblem(2) to child %d\n", myid, child);
		        MPI_Send(job, sizeof(Job), MPI_BYTE, child, SUBPROBLEM_TAG, MPI_COMM_WORLD);
		        // active_list[(status.MPI_SOURCE % (myid * MAX_CHILDREN)) - 1] = true;
		        printf(" --- SLAVE %d: sent subproblem(2) to child %d\n", myid, child);
				continue;
			}
			if (status.MPI_TAG == CUTOFF_TAG) {
				MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &status);
				printf(" --- SLAVE %d: received a cutoff from %d\n", myid, status.MPI_SOURCE);
				if (status.MPI_SOURCE != master) {
					printf(" --- SLAVE %d: false cutoff\n", myid);
				} else {
					send_cutoff(children_list, n_children);
					MPI_Send(&myid, 1, MPI_INT, status.MPI_SOURCE, CUTOFF_ACK_TAG, MPI_COMM_WORLD);
					// is_cutoff = true;
					break;
				}
			}
			if (status.MPI_TAG == TERMINATION_TAG) {
    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &status);
    			// for (i=0; i<slaves; i++) {
    			// 	if (slave_list[i]) {
    			// 		MPI_Send(&myid, 1, MPI_INT, i, TERMINATION_TAG, MPI_COMM_WORLD);
    			// 		MPI_Recv(&from, 1, MPI_INT, i, TERMINATION_TAG, MPI_COMM_WORLD, &status);
    			// 		slave_list[i] = false;
    			// 	}
    			// }
    			after = wall_clock_time();
    			comm_time += after - before;
    			fprintf(stderr, " --- (premature) SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0);
    			MPI_Send(&myid, 1, MPI_INT, master, TERMINATION_TAG, MPI_COMM_WORLD);
    			return;
			}
			if (status.MPI_TAG == SUBPROBLEM_TAG) {
				MPI_Recv(job, sizeof(Job), MPI_BYTE, MPI_ANY_SOURCE, SUBPROBLEM_TAG, MPI_COMM_WORLD, &status);
				// MPI_Send(&idle, 1, MPI_INT, status.MPI_SOURCE, STATUS_TAG, MPI_COMM_WORLD);
			}
		}

		after = wall_clock_time();
		comm_time += after - before;
		printf(" --- SLAVE %d: one iteration of alphabeta\n", myid);
		before = wall_clock_time();
        Tuple *ret = alphabeta(opponent(player),
                make_move(move_list[i], player, copy_board(board)),
                depth-1,
                -beta,
                -alpha);
        if (-(ret->score) >= alpha) {
            alpha = -(ret->score);
        }
        after = wall_clock_time();
        comp_time += after - before;
	}
	// subproblem finished
	// TODO: send RETURN to master
	// if (is_cutoff) {
	// 	printf(" --- SLAVE %d: cutoff encountered\n", myid);
	// 	is_cutoff = false;
	// 	continue;
	// }
	before = wall_clock_time();
	while (has_active_children(active_list, n_children)) {
		printf(" --- SLAVE %d: waiting for result\n", myid);
		// print_slave_list(slave_list);
		MPI_Recv(result, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
		// printf(" --- SLAVE %d: received a result from %d\n", myid, status.MPI_SOURCE);
		active_list[(status.MPI_SOURCE % (myid * MAX_CHILDREN)) - 1] = false;
		// TODO: 
		if (-result->score >= alpha) {
			alpha = -result->score;
		}
	}
	result->score = alpha;
	result->move = parent_move;
	MPI_Send(result, sizeof(Tuple), MPI_BYTE, master, RETURN_TAG, MPI_COMM_WORLD);
	printf(" --- SLAVE %d: sent a result to %d\n", myid, master);
	comm_time += wall_clock_time() - before;
	// send_return(master, tuple(alpha, best_move));
	// idle = true;
	// MPI_Send(&myid, 1, MPI_INT, MASTER_ID, SET_INACTIVE_TAG, MPI_COMM_WORLD);
	// printf(" --- SLAVE %d: send inactive status to %d\n", myid, MASTER_ID);
	// send_random_request();
	// idle = true;
}

void slave()
{
	int score, slave, n_moves, i, from, n_children;
	bool idle = true, has_message = false;
	long long before, after;
	int alpha, beta, depth, parent_move;
	char player, *board;

	int *move_list;
	int *children_list = create_children_list(&n_children);
	int *active_list = malloc(sizeof(int) * n_children);
	for (i=0;i<n_children;i++) active_list[i] = false;
	int master = (myid-1) / MAX_CHILDREN;
	Job *job = malloc(sizeof(Job));
	Tuple *result = malloc(sizeof(Tuple));
	// int dirty = false;
	// TODO: enforce evaluation limit

	while (true) {
		before = wall_clock_time();

		while (true) {
			MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			if (status.MPI_TAG == SUBPROBLEM_TAG) {
				MPI_Recv(job, sizeof(Job), MPI_BYTE, master, SUBPROBLEM_TAG, MPI_COMM_WORLD, &status);
				printf(" --- SLAVE %d: received a subproblem from %d\n", myid, status.MPI_SOURCE);
				// MPI_Send(&idle, 1, MPI_INT, status.MPI_SOURCE, STATUS_TAG, MPI_COMM_WORLD);
				// master = status.MPI_SOURCE;
				break;

			} else if (status.MPI_TAG == TERMINATION_TAG) {
    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &status);
    			after = wall_clock_time();
    			comm_time += after - before;
    			fprintf(stderr, " --- SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0);
    			MPI_Send(&myid, 1, MPI_INT, status.MPI_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD);
    			return;
			} else if (status.MPI_TAG == CUTOFF_TAG) {
    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &status);
    			printf(" --- SLAVE %d: received a cutoff from %d\n", myid, status.MPI_SOURCE);
    			MPI_Send(&myid, 1, MPI_INT, status.MPI_SOURCE, CUTOFF_ACK_TAG, MPI_COMM_WORLD);
			}
			else {
				MPI_Recv(job, sizeof(Job), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				printf(" --- SLAVE %d: received an unknown message (%d) from %d\n", myid, status.MPI_TAG, status.MPI_SOURCE);
			}
		}

    	after = wall_clock_time();
    	comm_time += after - before;


    }	
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

	// MPI_Barrier(MPI_COMM_WORLD);

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


    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    slaves = nprocs;
	before = wall_clock_time();
    play(BLACK, 12);
    after = wall_clock_time();
    if (myid == MASTER_ID) {
    	fprintf(stderr, " --- PARALLEL: total_elapsed_time=%6.2f seconds\n", (after - before) / 1000000000.0);
    }
    MPI_Finalize();

#else
    before = wall_clock_time();
    play_serial(WHITE, 12);
    after = wall_clock_time();
    fprintf(stderr, " --- SERIAL: total_elapsed_time=%6.2f seconds\n", (after - before) / 1000000000.0);
#endif
    return 0;
}