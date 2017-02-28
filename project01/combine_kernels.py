import numpy as np

def combine(k1, k2):
	res = np.zeros(k1.shape)
	for x in xrange(len(k1)):
		for y in xrange(len(k1[0])):
			magnitude = 0.
			for i in xrange(len(k2)):
				for j in xrange(len(k2[0])):
					xn = x + i - 1
					yn = y + j - 1
					if xn < 0 or yn < 0 or xn > len(k1)-1 or yn > len(k1[0])-1:
						continue
					print '\t',
					print 'xn:',xn,',','yn:',yn
					print '\t',
					print 'i:',i,',','j:',j
					print
					magnitude += k1[xn][yn] * k2[xn][yn]
			res[x][y] = magnitude
	return res

k1 = np.array([[1./9,1./9,1./9],[1./9,1./9,1./9],[1./9,1./9,1./9]])
k2 = np.array([[1./9,1./9,1./9],[1./9,1./9,1./9],[1./9,1./9,1./9]])

k3 = np.array([[-1,0,1],[-2,0,2],[-1,0,1]])
k4 = np.array([[-1,-2,-1],[0,0,0],[1,2,1]])
k5 = np.array([[0,-1,0],[-1,5,-1],[0,-1,0]])
k6 = np.array([[-1,-1,-1],[-1,8,-1],[-1,-1,-1]])

print combine(k1, k2)