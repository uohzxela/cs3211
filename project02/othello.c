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
#define COMPUTATION_TAG 1
#define RESULT_TAG 2

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
    // printf("alpha: %d\n", alpha);
    // printf("beta: %d\n", beta);
    // printf("\n");

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
            // printf("alpha: %d\n", alpha);
            // printf("score: %d\n", score);
            // printf("move: %s\n", index2label(moves[i]));
            // printf("\n");
            alpha = score;
            best_move = moves[i];
        }
    }

    // if (best_move < 0) {
    // 	// opponent also doesn't have any legal moves
    // 	if (!any_legal_move(opponent(player), board)) {
    // 		return tuple(final_value(player, board), -1);
    // 	}
    // 	// continue to recurse down without making any moves for current player
    // 	Tuple *ret = alphabeta(opponent(player), board, depth-1, -beta, -alpha);
    // 	return tuple(-(ret->score), -1);
    // }
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

#ifdef MPI_ENABLED
int master(char player, char *board, int depth)
{

	int best_move = -1;
	int score, nmoves, i, j;
    int alpha = -9999, beta = 9999;

    int current_move[slaves];
    int *moves = generate_moves(player, board, &nmoves);
    if (moves == NULL) {
    	printf("no more moves\n");
    	return best_move;
    }
    bool finished[slaves];
    for(i = 0; i < slaves; i++) {
        finished[i] = false;
    }

    Tuple *ret = alphabeta(opponent(player),
            make_move(moves[0], player, copy_board(board)),
            depth-1,
            -beta,
            -alpha);
    alpha = -(ret->score);
    best_move = moves[0];

    Job *job = malloc(sizeof(Job));
    printf("num of moves: %d\n", nmoves);
    printf("slaves: %d\n", slaves);
    printf("job size: %lu\n", sizeof(Job));

    for (i=0; i<min(nmoves, slaves); i++) {
        job->player = opponent(player);
        memcpy(job->board, make_move(moves[i], player, copy_board(board)), sizeof(char)*SQUARES);
        job->depth = depth-1;
        job->alpha = -beta;
        job->beta = -alpha;

        printf("send job to %i\n", i);
        MPI_Send(job, sizeof(Job), MPI_BYTE, i, COMPUTATION_TAG, MPI_COMM_WORLD);

        current_move[i] = moves[i];
    }

    for (i=slaves; i<nmoves; i++) {
        MPI_Recv(&score, 1, MPI_INT, MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, &status);
        printf("received score %i from %i\n", score, status.MPI_SOURCE);

        if (score >= alpha) {
            best_move = current_move[status.MPI_SOURCE];
            alpha = score;
        }

        memcpy(job->board, make_move(moves[i], player, copy_board(board)), sizeof(char)*SQUARES);
        job->player = opponent(player);
        job->depth = depth-1;
        job->alpha = -beta;
        job->beta = -alpha;

        printf("send job(2) to %i\n", status.MPI_SOURCE);
        MPI_Send(job, sizeof(Job), MPI_BYTE, status.MPI_SOURCE, COMPUTATION_TAG, MPI_COMM_WORLD);
        current_move[status.MPI_SOURCE] = moves[i];
    }

    // more slaves than moves
    job->finished = true;
    for (i=nmoves; i<slaves; i++) {
        if (finished[i])
            break;
        printf("prematurely finishing %i\n", i);
        MPI_Send(job, sizeof(Job), MPI_BYTE, i, COMPUTATION_TAG, MPI_COMM_WORLD);
        finished[i] = true;
    }
    // wait for the rest of results
    for(i = 0; i < min(nmoves, slaves); i++) {
        MPI_Recv(&score, 1, MPI_INT, MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, &status);
        if(score >= alpha) {
            alpha = score;
            best_move = current_move[status.MPI_SOURCE];
            // printf("best move updated: %d\n", best_move);
        }
        printf("received score %i from %i, alpha = %d\n", score, status.MPI_SOURCE, alpha);
    }
    // }
    job->finished = true;
    for (i=0; i<slaves; i++) {
        if (finished[i])
            break;
        printf("finishing %i\n", i);
        MPI_Send(job, sizeof(Job), MPI_BYTE, i, COMPUTATION_TAG, MPI_COMM_WORLD);
    }
    return best_move;
}
void slave()
{
	int score;
	Job *job = malloc(sizeof(Job));
	long long before, after;
    while (true) {
    	// communication
    	before = wall_clock_time();
        MPI_Recv(job, sizeof(Job), MPI_BYTE, MASTER_ID, COMPUTATION_TAG, MPI_COMM_WORLD, &status);
        after = wall_clock_time();
        if(job->finished) {
            // printf("job %d finished\n", myid);
            break;
        }
        comm_time += after - before;
        // computation
        before = wall_clock_time();
        score = -(alphabeta(job->player, job->board, job->depth, job->alpha, job->beta)->score);
        after = wall_clock_time();
        comp_time += after - before;

        before = wall_clock_time();
        MPI_Send(&score, 1, MPI_INT, MASTER_ID, RESULT_TAG, MPI_COMM_WORLD);
        after = wall_clock_time();
        comm_time += after - before;
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