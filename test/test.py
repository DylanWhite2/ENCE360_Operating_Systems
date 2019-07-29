import sys
import subprocess
import os
import time
import csv
import numpy as np

num_tests = 10
timestr = time.strftime("%Y%m%d-%H%M%S")
threads = np.linspace(1, 32, 17, dtype = int)
files = ['test.txt', 'small.txt', 'large.txt']

with open(timestr + 'test', 'wb') as myfile:
	wr = csv.writer(myfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
	wr.writerow(['Test', 'File', 'Threads', 'Time'])
	
	for jj in range(0, len(threads)):	
		for ii in range(0, len(files)):  
			for kk in range(0, num_tests):
			timeStart = time.time()
			subprocess.call(["./downloader", str(files[ii]), str(threads[jj]), "download"])
			#subprocess.call(["./queue_test"])
			timeEnd = time.time()
			exe_time = timeEnd - timeStart
			wr.writerow([kk + 1, files[ii], threads[jj], exe_time])
