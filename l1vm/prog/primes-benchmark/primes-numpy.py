import numpy as np
import math
import time
start_time = time.time()

n = 100000000
prime = np.ones(n + 1, dtype=bool)
prime[0:2] = False

for p in range(2, int(math.sqrt(n)) + 1):
    if prime[p]:
        prime[p*p:n+1:p] = False

print("--- %s seconds ---" % (time.time() - start_time))

for i in range(2, n + 1):
    if prime[i]:
        print(i, end="\n")
