import pandas as pd

NUM_SIZES = 5
NUM_THREADS = 40

data = [[0 for j in xrange(NUM_SIZES)] for i in xrange(NUM_THREADS)]
size_i = 0
threads_i = 0

SIZE_INDEX_MAP = {
	128: 0,
	256: 1,
	512: 2,
	1024: 3,
	2048: 4
}

with open('task4.in') as f:
	for line in f.readlines():
		line = line.strip()
		if not line:
			continue
		words = line.split(" ")
		if words[-1] == 'threads':
			size_i = SIZE_INDEX_MAP[int(words[-4])]
			threads_i = int(words[-2]) - 1
		else:
			time = words[-2]
			print 'size_i:', size_i
			print 'threads_i:', threads_i
			data[threads_i][size_i] = time

columns = ['128', '256', '512', '1024', '2048']
index = xrange(1, NUM_THREADS+1)

df = pd.DataFrame(data, index=index, columns=columns)

print df