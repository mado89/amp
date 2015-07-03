#!/bin/bash

#rm benchmark_results.csv
rm bench.log
#rm java_times.txt
rm -f results/*.log

mkdir -p results

repeats=30
loops=1

for proc in 16 32 64 128 256 512
do
	./benchmark.sh -n $repeats -c "./bin/test -l LockFreeSkipList -t $proc -r 600"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/sync.lfs.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test -l LazySkipList -t $proc -r 600"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/sync.ls.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test -l TestList -t $proc -r 600"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/sync.tl.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_simple -l LockFreeSkipList -t $proc -r 4000 -d 10"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/test.lfs.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_simple -l LazySkipList -t $proc -r 4000 -d 10"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/test.ls.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_simple -l TestList -t $proc -r 4000 -d 10"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/test.tl.$proc.log"
	rm bench.log
done

