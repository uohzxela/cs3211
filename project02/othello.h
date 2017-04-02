typedef int bool;
#define true 1
#define false 0

typedef struct _Tuple Tuple;

int find_bracket(int square, char player, char *board, int direction);
char opponent(char player);
bool is_legal(int move, char player, char *board);
bool is_valid(int move);
char* index2label(int index);
int label2index(const char *label);
char* initial_board();
void print_board(char *board);
char* copy_board(char *board);
int score(char player, char *board);
int get_move(char player, char *board);
int find_bracket(int square, char player, char *board, int direction);
void make_flips(int move, char player, char *board, int direction);
char* make_move(int move, char player, char *board);
int minimax_strategy(char player, char *board);
int final_value(char player, char *board);
Tuple* tuple(int score, int move);
Tuple* minimax(char player, char *board, int depth, int alpha, int beta);
void play(char color);