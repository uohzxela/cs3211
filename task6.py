import pandas as pd
import matplotlib.pyplot as plt

SIZES = [4 * (2**i) for i in xrange(16)]
TIMINGS = []

with open('task6.in') as f:
	for line in f.readlines():
		words = line.split(' ')
		TIMINGS.append(words[-2])

df = pd.DataFrame(TIMINGS, index=SIZES, columns=['timing'])
print df

plt.plot(SIZES, TIMINGS)
plt.ylabel('time')
plt.xlabel('size')
plt.show()