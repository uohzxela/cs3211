#include <stdio.h>
#include <stdlib.h>

typedef int bool;
#define true 1
#define false 0

static const int WIDTH = 10;
static const int HEIGHT = 10;

static const int ROWS = HEIGHT-2;
static const int COLS = WIDTH-2;
static const int SQUARES = WIDTH*HEIGHT;

static const char *INITIAL_WHITE_POS[] = {"b3", "b5", "b6", "c4", "c5", "d4", "d5", "d6","d7","e2","e3","e4","e6","f3","g3","g4","g6","h4","h5"};
static const char *INITIAL_BLACK_POS[] = {"a4", "a5", "a6", "b4", "c1","c3", "d1", "d2", "d3","e5", "e7", "e8","f2","f4","f5","g5","h3","h6"};

static const char EMPTY = '.';
static const char BLACK = '@';
static const char WHITE = 'o';
static const char OUTER = '?';

static const int UP = -WIDTH;
static const int DOWN = WIDTH;
static const int LEFT = -1;
static const int RIGHT = 1;
static const int UP_RIGHT = -(WIDTH-1);
static const int DOWN_RIGHT = WIDTH+1;
static const int DOWN_LEFT = WIDTH-1;
static const int UP_LEFT = -(WIDTH+1);

static const int DIRECTIONS[] = {UP, UP_RIGHT, RIGHT, DOWN_RIGHT, DOWN, DOWN_LEFT, LEFT, UP_LEFT};

int find_bracket(int square, char player, char *board, int direction);
char opponent(char player);
bool is_legal(int move, char player, char *board);
bool is_valid(int move);

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

char* initial_board() {
	char *board = malloc(sizeof(int)*(SQUARES));
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

int strategy(char player, char *board)
{
	return label2index("f6");
}

char* copy_board(char *board)
{
	char *copy = malloc(sizeof(int)*(SQUARES));
	size_t i;
	for (i=0; i<SQUARES; i++) {
		copy[i] = board[i];
	}
	return copy;
}

int score(char player, char *board)
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
	return mine - theirs;
}

bool is_legal(int move, char player, char *board)
{
	if (board[move] != EMPTY) return false;
	bool has_bracket = false;
	size_t i;
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

int get_move(char player, char *board)
{
	char *copy = copy_board(board);
	int move = strategy(player, copy);
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
	if (board[bracket] == player) return -1;

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

void make_move(int move, char player, char *board)
{
	board[move] = player;
	size_t i;
	for (i=0; i<sizeof(DIRECTIONS)/sizeof(DIRECTIONS[0]); i++) {
		int d = DIRECTIONS[i];
		make_flips(move, player, board, d);
	}
}

void play(char color)
{
	char *board = initial_board();
	printf("Before:");
	print_board(board);
	char player = color;
	int move = get_move(player, board);
	make_move(move, player, board);
	printf("After:");
	print_board(board);
	printf("Score: %d\n", score(color, board));
}

int main(int argc, char *argv[])
{
	play(WHITE);
	return 0;
}