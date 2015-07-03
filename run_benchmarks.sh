#!/bin/bash

#rm benchmark_results.csv
rm bench.log
#rm java_times.txt
rm -f results/*.log

mkdir -p results

repeats=30

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

	./benchmark.sh -n $repeats -c "./bin/test_correct_simple -l LockFreeSkipList -t $proc -r 100000"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/test.lfs.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_simple -l LazySkipList -t $proc -r 100000"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/test.ls.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_simple -l TestList -t $proc -r 100000"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/test.tl.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_range -l LockFreeSkipList -t $proc -r 10000 -d 10"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/range.lfs.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_range -l LazySkipList -t $proc -r 10000 -d 10"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/range.ls.$proc.log"
	rm bench.log

	./benchmark.sh -n $repeats -c "./bin/test_correct_range -l TestList -t $proc -r 10000 -d 10"
	grep "#bench" bench.log | sed 's/\#bench\#//g'  > "results/range.tl.$proc.log"
	rm bench.log
done

