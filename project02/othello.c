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

#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
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
#define MASTER_ID slaves

#define SUBPROBLEM_TAG 1
#define RESULT_TAG 2
#define REQUEST_TAG 3
#define CUTOFF_TAG 4
#define CANCELLATION_TAG 5
#define TERMINATION_TAG 6
#define SHARE_TAG 7
#define FIND_INACTIVE_TAG 8
#define SET_INACTIVE_TAG 9
#define STATUS_TAG 10
#define TERMINATED_STATUS_TAG 11
#define AVAILABILITY_TAG 12
#define IS_AVAILABLE_TAG 13
#define RESULT_ACK_TAG 14
#define ACQUIRE_TAG 15
#define ACQUIRE_ACK_TAG 16
#define RELEASE_TAG 17
#define RELEASE_ACK_TAG 18

#define PROC_NAME myid == MASTER_ID ? "MASTER" : "SLAVE"

#define MAX_CHILDREN 4

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

Result* result(int score, int move)
{
    Result *result = malloc(sizeof(Result));
    result->score = score;
    result->move = move;
    return result;
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

Result* alphabeta(char player, char *board, int depth, int alpha, int beta)
{
    if (depth == 0) {
        // TODO: change score() to evaluate()
        return result(evaluate(player, board), -1);
    }
    int n;
    int *moves = generate_moves(player, board, &n);
    // no legal moves for current player
    if (moves == NULL) {
        // opponent also doesn't have any legal moves
        if (!any_legal_move(opponent(player), board)) {
            return result(final_value(player, board), -1);
        }
        // continue to recurse down without making any moves for current player
        Result *ret = alphabeta(opponent(player), board, depth-1, -beta, -alpha);
        return result(-(ret->score), -1);
    }

    int best_move = -1;
    int score, i;

    for (i=0; i<n; i++) {
        if (alpha >= beta) break;
        Result *ret = alphabeta(opponent(player),
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
    return result(alpha, best_move);
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
		slave = rand() % (slaves);
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
		MPI_Recv(&from, 1, MPI_INT, children_list[i], CUTOFF_TAG, MPI_COMM_WORLD, &status);
		printf(" --- SLAVE %d: successfully cut off child %d\n", myid, children_list[i]);
	}

}

// void send_return(int master, Tuple *result)
// {
// 	MPI_Send(result, sizeof(Tuple), MPI_BYTE, master, RETURN_TAG, MPI_COMM_WORLD);
// }

bool is_slave_list_empty(int *slave_list)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i]) return false;
	}
	return true;
}

void print_slave_list(int *slave_list, int n_slaves)
{
	int i;
	printf(" --- SLAVE %d: slave list (%d) [", myid, n_slaves);
	for (i=0; i<slaves; i++) {
		printf(" %d ", slave_list[i]);
	}
	printf("]\n");
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
    int i, child_index, *children_list = malloc(sizeof(int) * MAX_CHILDREN);
    for (i=0; i<MAX_CHILDREN; i++) {
    	child_index = myid * MAX_CHILDREN + 1 + i;
    	if (child_index < slaves) {
    		children_list[(*n_children)++] = child_index;
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



void send_random_request() 
{
	int slave = random_slave();
	MPI_Send(&myid, 1, MPI_INT, slave, REQUEST_TAG, MPI_COMM_WORLD);
	// printf(" --- SLAVE %d: sent a request to %d\n", myid, slave);
}
void send_request_to_master()
{
	MPI_Send(&myid, 1, MPI_INT, MASTER_ID, REQUEST_TAG, MPI_COMM_WORLD);
	// printf(" --- SLAVE %d: sent a request to master\n", myid);
}
void receive_request(int* from)
{	
	MPI_Recv(from, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
	// printf(" --- SLAVE %d: received a request from %d\n", myid, *from);
}
void send_result(int to, Result *result)
{
	MPI_Send(result, sizeof(Result), MPI_BYTE, to, RESULT_TAG, MPI_COMM_WORLD);
	printf(" --- SLAVE %d: sent a result to %d\n", myid, to);
}
void receive_result(int *from, Result *result) 
{
	MPI_Recv(result, sizeof(Result), MPI_BYTE, MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
	printf(" --- SLAVE %d: received a result from %d\n", myid, *from);
}
void send_subproblem(int to, Job *job)
{
	MPI_Send(job, sizeof(Job), MPI_BYTE, to, SUBPROBLEM_TAG, MPI_COMM_WORLD);
	printf(" --- %s %d: sent a subproblem to %d\n", PROC_NAME, myid, to);
}
void receive_subproblem(int *from, Job *job)
{
	MPI_Recv(job, sizeof(Job), MPI_BYTE, MPI_ANY_SOURCE, SUBPROBLEM_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
	printf(" --- %s %d: received subproblem from %d\n", PROC_NAME, myid, *from);
}
void non_blocking_probe(int *has_message, MPI_Status *status)
{
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, has_message, status);
}
void probe(MPI_Status *status)
{
	// printf(" --- SLAVE %d: probing\n", myid);
	MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, status);
}
void send_cancellation(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, CANCELLATION_TAG, MPI_COMM_WORLD);
	// printf(" --- %s %d: sent a cancellation to %d\n", PROC_NAME, myid, to);
}
void receive_cancellation()
{
	int from;
	MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CANCELLATION_TAG, MPI_COMM_WORLD, &status);
	// printf(" --- SLAVE %d: received a cancellation from %d\n", myid, from);
}

void send_termination(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, TERMINATION_TAG, MPI_COMM_WORLD);
	printf(" --- MASTER: sent termination request to %d\n", to);
}
void receive_termination(int *from)
{
	MPI_Recv(from, 1, MPI_INT, MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &status);
	*from = status.MPI_SOURCE;
	printf(" --- SLAVE %d: received a termination request from %d\n", myid, *from);
}
void send_terminated_status(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, TERMINATED_STATUS_TAG, MPI_COMM_WORLD);
	printf(" --- SLAVE %d: sent terminated status to %d\n", myid, to);
}
void receive_terminated_status(int from)
{
	int tmp;
	MPI_Recv(&tmp, 1, MPI_INT, from, TERMINATED_STATUS_TAG, MPI_COMM_WORLD, &status);
	printf(" --- MASTER: received terminated status from %d\n", from);
}
int has_message(MPI_Status *status)
{
	int flag;
	non_blocking_probe(&flag, status);
	return flag;
}
void send_is_available(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, IS_AVAILABLE_TAG, MPI_COMM_WORLD);
	printf(" --- SLAVE %d: inquiring availability from %d\n", myid, to);
}
void receive_is_available(int *from)
{
	MPI_Recv(from, 1, MPI_INT, MPI_ANY_SOURCE, IS_AVAILABLE_TAG, MPI_COMM_WORLD, &status);
	printf(" --- SLAVE %d: received availability inquiry from %d\n", myid, *from);
}
void send_availability(int to, int availability)
{
	MPI_Send(&availability, 1, MPI_INT, to, AVAILABILITY_TAG, MPI_COMM_WORLD);
	printf(" --- SLAVE %d: sent availability to %d\n", myid, to);
}
void receive_availability(int from, int *availability)
{
	MPI_Recv(availability, 1, MPI_INT, from, AVAILABILITY_TAG, MPI_COMM_WORLD, &status);
	printf(" --- SLAVE %d: received availability from %d\n", myid, from);
}
void send_result_ack(int to)
{
	MPI_Send(&myid, 1, MPI_INT, to, RESULT_ACK_TAG, MPI_COMM_WORLD);
}
void receive_result_ack(int from)
{
	int tmp;
	MPI_Recv(&tmp, 1, MPI_INT, from, RESULT_ACK_TAG, MPI_COMM_WORLD, &status);
}
int is_available(int slave)
{
	send_is_available(slave);
	int availability;
	receive_availability(slave, &availability);
	return availability;
}
int* create_active_list(int n_children)
{
	int i, *active_list = malloc(sizeof(int) * n_children);
	for (i=0;i<n_children;i++) active_list[i] = false;
	return active_list;
}
// int get_master()
// {
// 	int master = (myid-1) / MAX_CHILDREN;
// 	if (master < 0) master = MASTER_ID;
// 	return master;
// }
void print_children_list(int *children_list, int n_children)
{
	int i;
	printf(" --- SLAVE %d: children [", myid);
	for (i=0; i<n_children; i++) {
		printf(" %d ", children_list[i]);
	}
	printf("]\n");
}
void print_move_list(int *move_list, int n_moves)
{
	int i;
	printf(" --- SLAVE %d: %d moves [", myid, n_moves);
	for (i=0; i<n_moves; i++) {
		printf(" %d ", move_list[i]);
	}
	printf("]\n");
}
void acquire_slave(int slave)
{
	long long time, time_before_acquire;
	while(true) {
		time_before_acquire = wall_clock_time();
		MPI_Send(&time_before_acquire, 1, MPI_LONG_LONG, slave, ACQUIRE_TAG, MPI_COMM_WORLD);
		MPI_Recv(&time, 1, MPI_LONG_LONG, slave, ACQUIRE_ACK_TAG, MPI_COMM_WORLD, &status);
		if (time < time_before_acquire) {
			continue;
		} else {
			break;
		}
	}
}	

void release_slave(int slave)
{
	long long time, time_before_release;
	while (true) {
		time_before_release = wall_clock_time();
		MPI_Send(&time_before_release, 1, MPI_LONG_LONG, slave, RELEASE_TAG, MPI_COMM_WORLD);
		MPI_Recv(&time, 1, MPI_LONG_LONG, slave, RELEASE_ACK_TAG, MPI_COMM_WORLD, &status);
		if (time < time_before_release) {
			continue;
		} else {
			break;
		}
	}
}
int master(char player, char *board, int depth)
{
    Job job;
	Result result;
	int i;

	job.player = player;
	memcpy(job.board, board, sizeof(char)*SQUARES);
	job.depth = depth;
	job.alpha = -9999;
	job.beta = 9999;

	int slave = 0;

	send_subproblem(slave, &job);
	receive_result(&slave, &result);

   	printf(" --- MASTER: received final result\n");
   	// exit(1);
   	for (i=0; i<slaves; i++) {
   		send_termination(i);
   		receive_terminated_status(i);
   	}
   	printf(" --- MASTER: all slaves terminated\n");
    return result.move;
}
int child_order(int child)
{
	if (myid * MAX_CHILDREN == 0)
		return child - 1;
	return (child % (myid * MAX_CHILDREN)) - 1;
}
void update_job(Job *job, int move, char player, char *board, int depth, int alpha, int beta)
{
    job->player = opponent(player);
    memcpy(job->board, make_move(move, player, copy_board(board)), sizeof(char)*SQUARES);
    job->depth = depth-1;
    job->alpha = -beta;
    job->beta = -alpha;
}
void add_slave(int *slave_list, int slave, int *n_slaves)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i] < 0) {
			slave_list[i] = slave;
			(*n_slaves)++;
			break;
		}
	}
}
void remove_slave(int *slave_list, int slave, int *n_slaves)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i] == slave) {
			slave_list[i] = -1;
			(*n_slaves)--;
			break;
		}
	}

}
int contains_slave(int *slave_list, int slave)
{
	int i;
	for (i=0; i<slaves; i++) {
		if (slave_list[i] == slave) {
			return 1;
		}
	}
	return 0;
}

int* acquire_slaves(int *n_slaves)
{
	long long time, time_before_acquire;
	*n_slaves = 0;
	int i, has_message, *slave_list = malloc(sizeof(int) * slaves);
	printf(" --- SLAVE %d: acquiring slaves\n", myid);
	for (i=0; i<slaves;i++) slave_list[i] = -1;
	time_before_acquire = wall_clock_time();
	for (i=0; i<slaves && *n_slaves < MAX_CHILDREN; i++) {
		if (myid == i) continue;
		MPI_Iprobe(MPI_ANY_SOURCE, ACQUIRE_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			
			MPI_Recv(&time, 1, MPI_LONG_LONG, MPI_ANY_SOURCE, ACQUIRE_TAG, MPI_COMM_WORLD, &status);
			time = -1;
			MPI_Send(&time, 1, MPI_LONG_LONG, status.MPI_SOURCE, ACQUIRE_ACK_TAG, MPI_COMM_WORLD);
		}
		MPI_Iprobe(MPI_ANY_SOURCE, ACQUIRE_ACK_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			MPI_Recv(&time, 1, MPI_LONG_LONG, i, ACQUIRE_ACK_TAG, MPI_COMM_WORLD, &status);
			if (time < time_before_acquire) {
				continue;
			} else {
				printf(" --- SLAVE %d: acquired slave %d\n", myid, i);
				slave_list[(*n_slaves)++] = i;
			}
		}		
		MPI_Send(&time_before_acquire, 1, MPI_LONG_LONG, i, ACQUIRE_TAG, MPI_COMM_WORLD);
		printf(" --- SLAVE %d: sent acquire to slave %d\n", myid, i);
		

	}

	return slave_list;
}	

void release_slaves(int *slave_list, int n_slaves)
{
	long long time, time_before_release;
	int i = 0;
	while (i<n_slaves) {
		time_before_release = wall_clock_time();
		MPI_Send(&time_before_release, 1, MPI_LONG_LONG, slave_list[i], RELEASE_TAG, MPI_COMM_WORLD);
		MPI_Recv(&time, 1, MPI_LONG_LONG, slave_list[i], RELEASE_ACK_TAG, MPI_COMM_WORLD, &status);
		if (time < time_before_release) {
			continue;
		} else {
			printf(" --- SLAVE %d: released slave %d\n", myid, slave_list[i]);
			i++;
		}
	}
}

Result* jamboree(char player, char *board, int depth, int alpha, int beta)
{
	if (depth == 0) {
		// printf(" --- SLAVE %d: depth is 0\n", myid);
		return result(evaluate(player, board), -1);
	}
	printf(" --- SLAVE %d: one iteration of jamboree, depth %d\n", myid, depth);
	int n_moves, score, best_move, i, from, has_message, has_result, has_termination, has_request;
	char *copy;
	int *move_list = generate_moves(player, board, &n_moves);

	if (!n_moves) {
		return result(evaluate(player, board), -1);
	}

	Job job;
	Result res;

	int n_children, child;
	int *children_list = create_children_list(&n_children);
	print_children_list(children_list, n_children);
	int *active_list = create_active_list(slaves);

	printf(" --- SLAVE %d: evaluate first move\n", myid);

	if (n_children) {
    	update_job(&job, move_list[0], player, board, depth, alpha, beta);
        send_subproblem(children_list[0], &job);
        receive_result(&from, &res);
        // printf("hello\n");
        score = -res.score;
	} else {
		copy = make_move(move_list[0], player, copy_board(board));
		printf(" --- SLAVE %d: starting alphabeta\n", myid);
		score = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
		printf(" --- SLAVE %d: finished alphabeta\n", myid);
	}


	if (score > alpha) {
		alpha = score;
		best_move = move_list[0];
	}
	if (alpha >= beta) {
		return result(alpha, best_move);
	}

	printf(" --- SLAVE %d: evaluate the rest of the moves\n", myid);
	// printf(" --- SLAVE %d: beginning to parallelize at depth %d\n", myid, depth);
    for (i=0; i<min(n_children, n_moves-1); i++) {
    	int move_index = i+1;
    	int child = children_list[i];
    	update_job(&job, move_list[move_index], player, board, depth, alpha, beta);
        send_subproblem(child, &job);
        active_list[child_order(child)] = true;
    }

	for (i=n_children; i<n_moves; i++) {
	// for (i=1; i<n_moves; i++) {
		// non_blocking_probe(&has_message, &status);
		// probe(&status);
		MPI_Iprobe(MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, &has_result, &status);
		if (has_result) {
			receive_result(&from, &res);
			// send_result_ack(from);
			if (-res.score > alpha) {
				alpha = -res.score;
				// FIXME: wrong move returned
				best_move = res.move;
			}

    		update_job(&job, move_list[i], player, board, depth, alpha, beta);
        	send_subproblem(from, &job);
        	continue;
		}
		MPI_Iprobe(MPI_ANY_SOURCE, TERMINATION_TAG, MPI_COMM_WORLD, &has_termination, &status);
		if (has_termination) {
			receive_termination(&from);
			send_terminated_status(from);
			return result(alpha, best_move);
		}

		MPI_Iprobe(MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &has_message, &status);
		if (has_message) {
			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &status);
			printf(" --- SLAVE %d: received a cutoff from %d\n", myid, status.MPI_SOURCE);
			if (status.MPI_SOURCE != master) {
				printf(" --- SLAVE %d: false cutoff\n", myid);
			} else {
				send_cutoff(children_list, n_children);
				MPI_Send(&myid, 1, MPI_INT, status.MPI_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD);
				// is_cutoff = true;
				break;
			}
		}

		copy = make_move(move_list[i], player, copy_board(board));
		score = -alphabeta(opponent(player), copy, depth-1, -beta, -alpha)->score;
		// printf("returned at depth %d\n", depth);
		if (score > alpha) {
			alpha = score;
			best_move = move_list[i];
		}

		if (alpha >= beta) {
			// TODO: send cutoff to children
			printf(" --- SLAVE %d: CUTOFF\n", myid);
    		send_cutoff(children_list, n_children);
			break;
		}
	}
	// printf(" --- SLAVE %d: aggregating\n", myid);
	// print_slave_list(slave_list, n_children);
	while (has_active_children(active_list, n_children)) {
		receive_result(&from, &res);
		if (-res.score > alpha) {
			alpha = -res.score;
			best_move = res.move;
		}
		active_list[child_order(from)] = false;
	}
	// printf(" --- SLAVE %d: finished aggregation\n", myid);

	// printf(" --- SLAVE %d: returning from depth %d\n", myid, depth);
	return result(alpha, best_move);
}

void slave()
{
	int from, master;

	Job job;
	Result *result;

	while (true) {
		while (true) {
			probe(&status);
			if (status.MPI_TAG == SUBPROBLEM_TAG) {
				receive_subproblem(&master, &job);
				break;
			} else if (status.MPI_TAG == TERMINATION_TAG) {
				receive_termination(&from);
				send_terminated_status(from);
			} else if (status.MPI_TAG == CUTOFF_TAG) {
    			MPI_Recv(&from, 1, MPI_INT, MPI_ANY_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD, &status);
    			printf(" --- SLAVE %d: received a cutoff from %d\n", myid, status.MPI_SOURCE);
    			MPI_Send(&myid, 1, MPI_INT, status.MPI_SOURCE, CUTOFF_TAG, MPI_COMM_WORLD);
			}
		}
		result = jamboree(job.player, job.board, job.depth, job.alpha, job.beta);
		send_result(master, result);
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

    slaves = nprocs-1;
	before = wall_clock_time();
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