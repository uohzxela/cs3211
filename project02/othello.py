########## initial params ##########

WIDTH = 10
HEIGHT = 10
ROWS = HEIGHT-2
COLS = WIDTH-2
SQUARES = WIDTH*HEIGHT

INITIAL_WHITE_POS = ['b3', 'b5', 'b6', 'c4', 'c5', 'd4','d5','d6','d7','e2','e3','e4','e6','f3','g3','g4','g6','h4','h5']
INITIAL_BLACK_POS = ['a4', 'a5', 'a6', 'b4', 'c1','c3', 'd1', 'd2', 'd3','e5', 'e7', 'e8','f2','f4','f5','g5','h3','h6']
# INITIAL_WHITE_POS = ['d4','e5']
# INITIAL_BLACK_POS = ['d5','e4']

####################################

EMPTY, BLACK, WHITE, OUTER = '.', '@', 'o', '?'
PIECES = (EMPTY, BLACK, WHITE, OUTER)
PLAYERS = {BLACK: 'Black', WHITE: 'White'}

UP, DOWN, LEFT, RIGHT = -WIDTH, WIDTH, -1, 1
UP_RIGHT, DOWN_RIGHT, DOWN_LEFT, UP_LEFT = -(WIDTH-1), WIDTH+1, (WIDTH-1), -(WIDTH+1)
DIRECTIONS = (UP, UP_RIGHT, RIGHT, DOWN_RIGHT, DOWN, DOWN_LEFT, LEFT, UP_LEFT)

def squares():
	return [i for i in xrange(WIDTH+1, WIDTH*(HEIGHT-1)-1) if 1 <= (i % WIDTH) <= COLS]

def label2index(label):
	col = ord(label[0]) - ord('a') + 1
	row = int(label[1:])
	return row*WIDTH+col

def index2label(index):
	row = index/WIDTH
	col = chr(ord('a') + (index%WIDTH)-1)
	return str(col) + str(row)

def initial_board():
   	board = [OUTER] * (WIDTH * HEIGHT)
   	for i in squares():
   		board[i] = EMPTY
   	for pos in INITIAL_WHITE_POS:
   		board[label2index(pos)] = WHITE
   	for pos in INITIAL_BLACK_POS:
   		board[label2index(pos)] = BLACK
   	return board

def print_row_label(row):
	if row > 9:
		print str(row),
	else:
		print str(row) + " ",

def print_board(board):
	print
	print "    ",
	print " ".join([chr(ord('a')+i) for i in xrange(COLS)]),
	for i in xrange(len(board)):
		c = board[i]
		if c == OUTER: c = ' '
		if i % WIDTH == 0:
			print
			print_row_label((i / WIDTH)),

		print c,
		if (i+1) % WIDTH == 0:
			print_row_label((i / WIDTH)),

	print
	print "    ",
	print " ".join([chr(ord('a')+i) for i in xrange(COLS)]),
	print

def is_valid(move):
    return 0 <= move < SQUARES

def opponent(player):
    return BLACK if player is WHITE else WHITE

def find_bracket(square, player, board, direction):
    bracket = square + direction
    # return None if there's nothing in between the brackets
    if board[bracket] == player:
        return None
    opp = opponent(player)
    while board[bracket] == opp:
        bracket += direction
    return None if board[bracket] in (OUTER, EMPTY) else bracket

def is_legal(move, player, board):
    has_bracket = lambda direction: find_bracket(move, player, board, direction)
    return board[move] == EMPTY and any(map(has_bracket, DIRECTIONS))

def make_move(move, player, board):
    board[move] = player
    for d in DIRECTIONS:
        make_flips(move, player, board, d)
    return board

def make_flips(move, player, board, direction):
    bracket = find_bracket(move, player, board, direction)
    if not bracket:
        return
    square = move + direction
    while square != bracket:
        board[square] = player
        square += direction

class IllegalMoveError(Exception):
    def __init__(self, player, move, board):
        self.player = player
        self.move = move
        self.board = board
    def __str__(self):
        return '%s cannot move to square %d' % (PLAYERS[self.player], self.move)

def legal_moves(player, board):
    return [sq for sq in squares() if is_legal(sq, player, board)]

def any_legal_move(player, board):
    return any(is_legal(sq, player, board) for sq in squares())

def play(color):
    board = initial_board()
    print "initial:",
    print_board(board)
    player = color
    move = get_move(player, board)
    make_move(move, player, board)
    print "after:",
    print_board(board)
    print "score:", score(color, board)

def get_move(player, board):
    copy = list(board) # copy the board to prevent cheating
    move = minimax_strategy(player, copy)
    print "move:", index2label(move)
    if not is_valid(move) or not is_legal(move, player, board):
        raise IllegalMoveError(player, move, copy)
    return move

def score(player, board):
    mine, theirs = 0, 0
    opp = opponent(player)
    for sq in squares():
        piece = board[sq]
        if piece == player: mine += 1
        elif piece == opp: theirs += 1
    return mine - theirs

def score_move(move):
    return score(player, make_move(move, player, list(board)))

def strategy(player, board):
	max_score = float("-inf")
	best_move = None
	print "legal moves:", map(index2label, legal_moves(player, board))
	for move in legal_moves(player, board):
		curr_score = score(player, make_move(move, player, list(board)))
		if max_score < curr_score:
			max_score = curr_score
			best_move = move
	return best_move

def minimax_strategy(player, board):
	return minimax(player, board, 5)[1]

MAX_VALUE = float('inf')
MIN_VALUE = -MAX_VALUE

def final_value(player, board):
    diff = score(player, board)
    if diff < 0:
        return MIN_VALUE
    elif diff > 0:
        return MAX_VALUE
    return diff

def minimax(player, board, depth):
	if depth == 0:
		return score(player, board), None
	moves = legal_moves(player, board)
	if not moves:
		if not any_legal_move(opponent(player), board):
			return final_value(player, board), None
		return -minimax(opponent(player), board, depth-1)[0], None
	max_val = -float('inf')
	best_move = None
	for m in moves:
		curr_val = -minimax(opponent(player), make_move(m, player, list(board)), depth-1)[0]
		if curr_val > max_val:
			max_val = curr_val
			best_move = m
	return max_val, best_move

play(WHITE)
