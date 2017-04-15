#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
// #include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include "othellox.h"

#ifdef MPI_ENABLED
#include "mpi.h"
MPI_Status status;
#endif

#define min(a,b) (a < b ? a : b)

#if 1
  #define DEBUG(a) printf a
#else
  #define DEBUG(a) (void)0
#endif

/** 
 * Initial parameters.
 **/

int rows, cols; // playable number of rows and columns
#define ROWS rows
#define COLS cols

char *INITIAL_WHITE_POS[256], *INITIAL_BLACK_POS[256]; // initial white/black positions
int n_initial_whites, n_initial_blacks;

int TIMEOUT;


/** 
 * Evaluation parameters.
 **/
int MAX_DEPTH;
long MAX_BOARDS;
long boards_evaluated = 0;


/**
 * The extra two rows and columns form the border of the game board.
 * The border will enclose the board inside a larger array,
 * so that illegal squares are "off" the edge and are easily detectable.
 **/
#define HEIGHT (ROWS+2)
#define WIDTH (COLS+2)
#define SQUARES WIDTH*HEIGHT

// constant variables related to the 8 directions a move can take
int *DIRECTIONS;
#define N_DIRECTIONS 8
#define UP -WIDTH
#define DOWN WIDTH
#define LEFT -1
#define RIGHT 1
#define UP_RIGHT -(WIDTH-1)
#define DOWN_RIGHT (WIDTH+1)
#define DOWN_LEFT (WIDTH-1)
#define UP_LEFT -(WIDTH+1)


// symbols to represent a state of a game board square
char EMPTY = '.';
char BLACK = '@';
char WHITE = 'o';
char OUTER = '?';
char PLAYER;

int myid, slaves = 1, nprocs;

#define MASTER_ID 0
#define PROC_NAME (myid == MASTER_ID ? "MASTER" : "SLAVE")
#define PARENT_ID ((myid-1) / MAX_CHILDREN)

// tag constants to be used for MPI communication
#define SUBPROBLEM_TAG 1
#define RETURN_TAG 2
#define REQUEST_TAG 3
#define CUTOFF_TAG 4
#define CUTOFF_ACK_TAG 5
#define TERMINATION_TAG 6
#define TERMINATION_ACK_TAG 7

#define MAX_CHILDREN 4
#define MAX_BOARDS_PER_SLAVE MAX_BOARDS/slaves

// for time statistics
long long comm_time = 0, comp_time = 0, before, after;
double AGGREGATION_TIMEOUT = 3.0;

struct _Tuple {
    int move;
    int score;
};

struct _Job {
    int alpha;
    int beta;
    char player;
    int depth;
    int move;
    char board[];
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
    if (index < 0) return "na";
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
    for (i=0; i<n_initial_whites; i++) {
        board[label2index(INITIAL_WHITE_POS[i])] = WHITE;
    }
    for (i=0; i<n_initial_blacks; i++) {
        board[label2index(INITIAL_BLACK_POS[i])] = BLACK;
    }
    return board;
}

void print_board(char *board)
{
    int i;
    DEBUG(("\n"));
    DEBUG(("     "));
    for (i=0; i<COLS; i++) { DEBUG(("%c ", 'a'+i)); }
    for (i=0; i<SQUARES; i++) {
        char c = board[i];
        if (c == OUTER) c = ' ';
        if (i % WIDTH == 0) {
            DEBUG(("\n"));
            DEBUG(("%d %c", i/WIDTH, (i/WIDTH > 9) ? '\0' : ' '));
        }
        DEBUG(("%c ", c));
        if ((i+1)%WIDTH == 0) {
            DEBUG(("%d %c", i/WIDTH, (i/WIDTH > 9) ? '\0' : ' '));
        }
    }
    DEBUG(("\n"));
    DEBUG(("     "));
    for (i=0; i<COLS; i++) { DEBUG(("%c ", 'a'+i)); }
    DEBUG(("\n"));
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
   	boards_evaluated++;
    return mine - theirs;
}

bool is_legal(int move, char player, char *board)
{
    if (!is_valid(move) || board[move] != EMPTY) return false;
    int i;
    for (i=0; i<N_DIRECTIONS; i++) {
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
    DEBUG(("Move: %s\n", index2label(move)));
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

    for (i=0; i<N_DIRECTIONS; i++) {
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
    if (depth == 0 || boards_evaluated >= MAX_BOARDS_PER_SLAVE) {
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
void print_best_move(int best_move)
{
	printf("Best moves: { %s }\n", index2label(best_move));
}

void play_serial(char player, int depth)
{
    char *board = create_board();
    DEBUG(("Before score: %d\n", evaluate(player, board)));
    print_board(board);
    int move = get_move(player, board, depth);
    print_best_move(move);
    make_move(move, player, board);
    DEBUG(("After score: %d\n", evaluate(player, board)));
    print_board(board);
}

#ifdef MPI_ENABLED
void send_cutoff(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, CUTOFF_TAG, MPI_COMM_WORLD);
	DEBUG((" --- SLAVE %d: cutting off child %d\n", myid, to));
}
void receive_cutoff_ack(int from)
{
	int tmp;
	MPI_Recv(&tmp, 1, MPI_INT, from, CUTOFF_ACK_TAG, MPI_COMM_WORLD, &status);
	DEBUG((" --- SLAVE %d: successfully cut off child %d\n", myid, from));
}
void send_cutoff_to_children(int *children_list, int n_children)
{
	int i;
	for (i=0; i<n_children; i++) {
		send_cutoff(children_list[i]);
		// receive_cutoff_ack(children_list[i]);
	}
}

void send_return(int to, Tuple *result)
{
	MPI_Send(result, sizeof(Tuple), MPI_BYTE, to, RETURN_TAG, MPI_COMM_WORLD);
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
void print_move_list(int *move_list, int n_moves)
{
    int i;
    DEBUG(("n_moves (%d): [", n_moves));
    for (i=0;i<n_moves;i++) {
        DEBUG(("%s ", index2label(move_list[i])));
    }
    DEBUG(("]\n"));
}

void update_job(Job *job, int move, char player, char *board, int depth, int alpha, int beta)
{
    job->player = opponent(player);
    memcpy(job->board, make_move(move, player, copy_board(board)), sizeof(char)*SQUARES);
    job->depth = depth-1;
    job->alpha = -beta;
    job->beta = -alpha;
    job->move = move;
}

void send_subproblem(Job *job, int to)
{
	MPI_Send(job, sizeof(Job)+SQUARES, MPI_BYTE, to, SUBPROBLEM_TAG, MPI_COMM_WORLD);
	DEBUG((" --- %s %d: sent subproblem to %d\n", PROC_NAME, myid, to));
}
void receive_result(Tuple *result, int *from)
{
	DEBUG((" --- %s %d: waiting for results\n", PROC_NAME, myid));
	MPI_Recv(result, sizeof(Tuple), MPI_BYTE, MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
	DEBUG((" --- %s %d: received result from %d\n", PROC_NAME, myid, *from));
}
void send_termination(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, TERMINATION_TAG, MPI_COMM_WORLD);
	// DEBUG((" --- MASTER: sent termination to %d\n", to));
}
void receive_termination_ack(int from)
{
	int tmp;
	MPI_Recv(&tmp, 1, MPI_INT, from, TERMINATION_ACK_TAG, MPI_COMM_WORLD, &status);
}
void probe(MPI_Status *status)
{
	MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, status);
}
void receive_subproblem(Job *job, int from)
{
	MPI_Recv(job, sizeof(Job)+SQUARES, MPI_BYTE, from, SUBPROBLEM_TAG, MPI_COMM_WORLD, &status);
	DEBUG((" --- SLAVE %d: received a subproblem from %d\n", myid, from));
}
void receive_termination(int *from)
{
	int tmp;
	MPI_Recv(&tmp, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
}
void send_termination_ack(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, TERMINATION_ACK_TAG, MPI_COMM_WORLD);
}
void print_slave_stats()
{
	DEBUG((" --- SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds; boards_evaluated:%ld\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0, boards_evaluated));
	printf("%d, %6.2f, %6.2f, %ld\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0, boards_evaluated);
}
void receive_cutoff(int *from)
{
	int tmp;
	MPI_Recv(&tmp, 1, MPI_INT, MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
	DEBUG((" --- SLAVE %d: received a cutoff from %d\n", myid, *from));
}
void send_cutoff_ack(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, CUTOFF_ACK_TAG, MPI_COMM_WORLD);
}
void send_result_to_parent(int score, int move)
{
	Tuple result = {.score = score, .move = move};
	MPI_Send(&result, sizeof(Tuple), MPI_BYTE, PARENT_ID, RETURN_TAG, MPI_COMM_WORLD);
}
int child_order(int child)
{	
	if ((myid * MAX_CHILDREN) == 0) return child-1;
	return (child % (myid * MAX_CHILDREN)) - 1;
}
int* create_active_list(int n_children)
{
	int i, *active_list = malloc(sizeof(int) * n_children);
	for (i=0;i<n_children;i++) active_list[i] = false;
	return active_list;
}
int master(char player, char *board, int depth)
{
    int n_moves, i, n_children, child;
    int *move_list = generate_moves(player, board, &n_moves);
    print_move_list(move_list, n_moves);
    int *children_list = create_children_list(&n_children);
    int *active_list = create_active_list(n_children);

    int alpha = -9999, beta = 9999;
    int best_move = -1, has_message;

    Job *job = malloc(sizeof(Job)+SQUARES);
	Tuple *result = malloc(sizeof(Tuple));

    for (i=0; i<min(n_moves, n_children); i++) {
    	int child = children_list[i];
        update_job(job, move_list[i], player, board, depth, alpha, beta);
        send_subproblem(job, child);
        active_list[child_order(child)] = true;
    }
    // if num of children < num of moves
    for (i=n_children; i<n_moves; i++) {

        receive_result(result, &child);

        if (-result->score > alpha) {
            best_move = result->move;
            alpha = -result->score;
        }
		if (alpha >= beta) {
			send_cutoff_to_children(children_list, n_children);
			break;
		}
        update_job(job, move_list[i], player, board, depth, alpha, beta);
        send_subproblem(job, child);

    }
	time_t start = time(NULL), end;
	while (has_active_children(active_list, n_children) && difftime(end, start) < AGGREGATION_TIMEOUT) {
		MPI_Iprobe(MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			receive_result(result, &child);
			active_list[child_order(child)] = false;
			if (-result->score > alpha) {
				alpha = -result->score;
			}
		}
		end = time(NULL);
	}

   	DEBUG((" --- MASTER: received final result\n"));

   	for (i=1; i<nprocs; i++) {
   		send_termination(i);
   		send_termination(i);
   		receive_termination_ack(i);
   	}

    return best_move;
}

void parallelize_alphabeta(Job *job, int *children_list, int n_children, int *active_list)
{
	char player = job->player;
	char *board = job->board;
	int alpha = job->alpha;
	int beta = job->beta;
	int depth = job->depth;
	int parent_move = job->move;

	if (depth <= 0 || boards_evaluated >= MAX_BOARDS_PER_SLAVE) {
    	send_result_to_parent(evaluate(player, board), parent_move);
    	return;
	}
	
	before = wall_clock_time();

	int n_moves, i, has_message, child, from, score;
	int *move_list = generate_moves(player, board, &n_moves);
	char *copy;

	// if (n_moves > 0) {
	//     copy = make_move(move_list[0], player, copy_board(board));
	//     alpha = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
	// 	comp_time += wall_clock_time() - before;
	// } else {
 //    	send_result_to_parent(evaluate(player, board), parent_move);
 //    	return;
	// }

	// if (alpha >= beta) {
 //    	send_result_to_parent(alpha, parent_move);
 //    	return;
	// }

	Tuple result;

	before = wall_clock_time();
    for (i=0; i<min(n_children, n_moves-1); i++) {
    	int move_index = i+1;
    	int child = children_list[i];
    	update_job(job, move_list[move_index], player, board, depth, alpha, beta);
    	send_subproblem(job, child);
        active_list[child_order(child)] = true;
    }
    comm_time += wall_clock_time() - before;

	for (i=n_children; i<n_moves; i++) {
		if (alpha >= beta) {
			send_cutoff_to_children(children_list, n_children);
			break;
		}
		before = wall_clock_time();
		MPI_Iprobe(MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			receive_termination(&from);
			send_termination_ack(from);
			comm_time += wall_clock_time() - before;
			print_slave_stats();
			return;
		}
		MPI_Iprobe(MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			receive_cutoff(&from);
			send_cutoff_to_children(children_list, n_children);
			send_cutoff_ack(from);
			break;
		}
		MPI_Iprobe(MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			receive_result(&result, &child);
			if (-(result.score) > alpha) {
				alpha = -(result.score);
			}
			update_job(job, move_list[i], player, board, depth, alpha, beta);
			send_subproblem(job, child);
			continue;
		}


		comm_time += wall_clock_time() - before;
		DEBUG((" --- SLAVE %d: one iteration of alphabeta\n", myid));
		before = wall_clock_time();
		copy = make_move(move_list[i], player, copy_board(board));
		score = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
        if (score > alpha) {
            alpha = score;
        }
        comp_time += wall_clock_time() - before;
	}

	before = wall_clock_time();
	time_t start = time(NULL), end;
	while (has_active_children(active_list, n_children) && difftime(end, start) < AGGREGATION_TIMEOUT) {
		MPI_Iprobe(MPI_ANY_SOURCE, RETURN_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			receive_result(&result, &child);
			active_list[child_order(child)] = false;
			if (-result.score > alpha) {
				alpha = -result.score;
			}
		}
		MPI_Iprobe(MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			receive_termination(&from);
			send_termination_ack(from);
			comm_time += wall_clock_time() - before;
			print_slave_stats();
			return;
		}
		end = time(NULL);
	}
	send_result_to_parent(alpha, parent_move);
	comm_time += wall_clock_time() - before;
}
void slave() {
	int n_children, from, has_message;
	int *children_list = create_children_list(&n_children);
	int *active_list = create_active_list(n_children);

	Job *job = malloc(sizeof(Job)+SQUARES);

	while (true) {
		before = wall_clock_time();
		while (true) {
			MPI_Iprobe(MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &has_message, &status);
			if (has_message) {
    			receive_termination(&from);
    			send_termination_ack(from);
    			comm_time += wall_clock_time() - before;
    			print_slave_stats();
    			return;
			}
			MPI_Iprobe(MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &has_message, &status);
			if (has_message) {
				receive_cutoff(&from);
				send_cutoff_ack(from);
			}
			MPI_Iprobe(MPI_ANY_SOURCE, SUBPROBLEM_TAG, MPI_COMM_WORLD, &has_message, &status);
			if (has_message) {
				receive_subproblem(job, PARENT_ID);
				break;
			}
		}
    	comm_time += wall_clock_time() - before;

    	parallelize_alphabeta(job, children_list, n_children, active_list);
  //   	player = job->player;
  //   	board = job->board;
  //   	alpha = job->alpha;
  //   	beta = job->beta;
  //   	depth = job->depth;
  //   	parent_move = job->move;


  //   	if (depth <= 0 || boards_evaluated >= MAX_BOARDS_PER_SLAVE) {
	 //    	send_result_to_parent(evaluate(player, board), parent_move);
	 //    	continue;
  //   	}
    	
  //   	before = wall_clock_time();
  //   	move_list = generate_moves(player, board, &n_moves);

		// // if (n_moves > 0) {
		// //     copy = make_move(move_list[0], player, copy_board(board));
		// //     alpha = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
		// // 	comp_time += wall_clock_time() - before;
		// // } else {
	 // //    	send_result_to_parent(evaluate(player, board), parent_move);
	 // //    	continue;
		// // }

		// // if (alpha >= beta) {
	 // //    	send_result_to_parent(alpha, parent_move);
	 // //    	continue;
		// // }

		// before = wall_clock_time();
	 //    for (i=0; i<min(n_children, n_moves-1); i++) {
	 //    	int move_index = i+1;
	 //    	int child = children_list[i];
	 //    	update_job(job, move_list[move_index], player, board, depth, alpha, beta);
	 //    	send_subproblem(job, child);
	 //        active_list[child_order(child)] = true;
	 //    }
	 //    comm_time += wall_clock_time() - before;

  //   	for (i=n_children; i<n_moves; i++) {
  //   		if (alpha >= beta) {
  //   			send_cutoff_to_children(children_list, n_children);
  //   			break;
  //   		}
  //   		before = wall_clock_time();
  //   		// printf("probing\n");
  //   		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &has_message, &status);

  //   		if (has_message) {
		// 		if (status.MPI_TAG == RETURN_TAG) {
		// 			receive_result(result, &child);
		// 			if (-(result->score) > alpha) {
		// 				alpha = -(result->score);
		// 			}
		// 			update_job(job, move_list[i], player, board, depth, alpha, beta);
		// 			send_subproblem(job, child);
		// 			continue;
		// 		}
		// 		if (status.MPI_TAG == CUTOFF_TAG) {
		// 			receive_cutoff(&from);
		// 			send_cutoff_to_children(children_list, n_children);
		// 			send_cutoff_ack(from);
		// 			break;
		// 		}
		// 		if (status.MPI_TAG == TERMINATION_TAG) {
		// 			receive_termination(&from);
	 //    			send_termination_ack(from);
	 //    			comm_time += wall_clock_time() - before;
	 //    			print_slave_stats();
	 //    			return;
		// 		}
  //   		}

  //   		comm_time += wall_clock_time() - before;
		// 	printf(" --- SLAVE %d: one iteration of alphabeta\n", myid);
  //   		before = wall_clock_time();
  //   		copy = make_move(move_list[i], player, copy_board(board));
  //   		score = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
	 //        if (score > alpha) {
	 //            alpha = score;
	 //        }
	 //        comp_time += wall_clock_time() - before;
  //   	}

  //   	before = wall_clock_time();
  //   	while (has_active_children(active_list, n_children)) {
  //   		receive_result(result, &child);
		// 	active_list[child_order(child)] = false;
		// 	if (-result->score > alpha) {
		// 		alpha = -result->score;
		// 	}
  //   	}
  //   	send_result_to_parent(alpha, parent_move);
  //   	comm_time += wall_clock_time() - before;
    }	
}

void play(char player, int depth)
{
	int best_move = -1;
	char *board = create_board();

	if (myid == MASTER_ID) {
	    DEBUG(("Before score: %d\n", evaluate(player, board)));
	    print_board(board);
		best_move = master(player, board, depth);
	} else {
		slave();
	}

	if (myid == MASTER_ID) {
		print_best_move(best_move);
	    make_move(best_move, player, board);
	    DEBUG(("After score: %d\n", evaluate(player, board)));
	    print_board(board);
	}
}
#endif

void parse_size(char *line)
{
	char *token;

	token = strsep(&line, ",");
	rows = atoi(token);

	token = strsep(&line, ",");
	cols = atoi(token);
}
void parse_white_positions(char *line)
{
	char *token, *positions;

	token = strsep(&line, " "); // token is now "{"
	token = strsep(&line, " "); // token is now "d5,e4" etc.

	positions = strdup(token);
	n_initial_whites = 0;
	while ((token = strsep(&positions, ","))) {
		INITIAL_WHITE_POS[n_initial_whites++] = token;
	}
	free(positions);
}
void parse_black_positions(char *line)
{
	char *token, *positions;

	token = strsep(&line, " "); // token is now "{"
	token = strsep(&line, " "); // token is now "d5,e4" etc.

	positions = strdup(token);
	n_initial_blacks = 0;
	while ((token = strsep(&positions, ","))) {
		INITIAL_BLACK_POS[n_initial_blacks++] = token;
	}
	free(positions);
}
void parse_color(char *line)
{
	char *p = line;
	// convert to lowercase
	for ( ; *p; ++p) *p = tolower(*p);
	if (strcmp(line, "black\n") == 0) {
		PLAYER = BLACK;
	} else {
		PLAYER = WHITE;
	}
}
void parse_timeout(char *line)
{
	TIMEOUT = atoi(line);
}
void parse_initialbrd(int argc, char* argv[])
{
    FILE *file; 
    char line[256];
    char *token, *line_dup;
    if (argc > 1) {
    	file = fopen(argv[1], "r");
    	while (fgets(line, sizeof(line), file)) {

			line_dup = strdup(line);
			token = strsep(&line_dup, " ");
			if (strcmp(token, "Size:") == 0) {
				parse_size(line_dup);
			} else if (strcmp(token, "White:") == 0) {
				parse_white_positions(line_dup);
			} else if (strcmp(token, "Black:") == 0) {
				parse_black_positions(line_dup);
			} else if (strcmp(token, "Color:") == 0) {
				parse_color(line_dup);
			} else if (strcmp(token, "Timeout:") == 0) {
				parse_timeout(line_dup);
			}
    	}
    	fclose(file);
    }
}
void parse_max_depth(char *line)
{
	MAX_DEPTH = atoi(line);
}
void parse_max_boards(char *line)
{
	MAX_BOARDS = atoi(line);
}
void parse_evalparams(int argc, char* argv[])
{
    FILE *file; 
    char line[256];
    char *token, *line_dup;
    if (argc > 2) {
    	file = fopen(argv[2], "r");
    	while (fgets(line, sizeof(line), file)) {

			line_dup = strdup(line);
			token = strsep(&line_dup, " ");
			if (strcmp(token, "MaxDepth:") == 0) {
				parse_max_depth(line_dup);
			} else if (strcmp(token, "MaxBoards:") == 0) {
				parse_max_boards(line_dup);
			}
    	}
    	fclose(file);
    }
}

void initialize_directions()
{
	DIRECTIONS = malloc(sizeof(int)*9);
	int i=0;
	DIRECTIONS[i++] = UP;
	DIRECTIONS[i++] = DOWN;
	DIRECTIONS[i++] = LEFT;
	DIRECTIONS[i++] = RIGHT;
	DIRECTIONS[i++] = UP_RIGHT;
	DIRECTIONS[i++] = DOWN_RIGHT;
	DIRECTIONS[i++] = DOWN_LEFT;
	DIRECTIONS[i++] = UP_LEFT;
}
int main(int argc, char *argv[])
{
	long long before, after;
	srand(time(0));
    parse_initialbrd(argc, argv);
    parse_evalparams(argc, argv);
	initialize_directions();
#ifdef MPI_ENABLED

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    slaves = nprocs;
	before = wall_clock_time();
    play(PLAYER, MAX_DEPTH);
    after = wall_clock_time();

    if (myid == MASTER_ID) {
    	DEBUG((" --- PARALLEL: total_elapsed_time=%6.2f seconds\n", (after - before) / 1000000000.0));
    	printf("Time: %6.2f\n", (after - before) / 1000000000.0);
    }
	MPI_Finalize();

#else
    before = wall_clock_time();
    play_serial(PLAYER, MAX_DEPTH);

    after = wall_clock_time();
    DEBUG((" --- SERIAL: total_elapsed_time=%6.2f seconds\n", (after - before) / 1000000000.0));
    printf("Time: %6.2f\n", (after - before) / 1000000000.0);
#endif
    return 0;
}