import pandas as pd
import matplotlib.pyplot as plt

SIZES = []
TIMINGS = []

with open('task8.in') as f:
	for line in f.readlines():
		words = line.split(' ')
		SIZES.append(words[2])
		TIMINGS.append(words[-2])

df = pd.DataFrame(TIMINGS, index=SIZES, columns=['timing'])
print df

plt.plot(SIZES, TIMINGS)
plt.ylabel('time')
plt.xlabel('size')
plt.show()