/*
 * test.cxx
 *
 *  Created on: Jun 18, 2015
 *      Author: martin
 */


#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>
#include <assert.h>
#include <getopt.h>
#include <sys/time.h>
#include <cstdint>
#include <signal.h>

#include "TestList.h"
#include "LazySkipList.h"
#include "LockFreeSkipList.h"

#include "synchrobench.h"

#include "synchrobench/c-cpp/src/atomic_ops/atomic_ops.h"

volatile AO_t stop;
unsigned int global_seed;
#ifdef TLS
__thread unsigned int *rng_seed;
#else /* ! TLS */
pthread_key_t rng_seed_key;
#endif /* ! TLS */
unsigned int levelmax;

/*void print_skiplist(sl_intset_t *set) {
  sl_node_t *curr;
  int i, j;
  int arr[levelmax];

  for (i=0; i< sizeof arr/sizeof arr[0]; i++) arr[i] = 0;

  curr = set->head;
  do {
    printf("%d", (int) curr->val);
    for (i=0; i<curr->toplevel; i++) {
      printf("-*");
    }
    arr[curr->toplevel-1]++;
    printf("\n");
    curr = curr->next[0];
  } while (curr);
  for (j=0; j<levelmax; j++)
    printf("%d nodes of level %d\n", arr[j], j);
}*/

SkipList* sl_set_new(int type)
{
	SkipList *set;

	if( type == 0)
		set= new TestList(levelmax);
	else if( type == 1 )
		set= new LazySkipList(levelmax);
	else if( type == 2 )
		set= new LockFreeSkipList(levelmax);
	else
		set= NULL;

	return set;
}

void sl_set_delete(SkipList *set) {
	delete set;
}

int sl_set_size(SkipList *set)
{
	return set->size();
}

int sl_add(SkipList *set, val_t val, int transactional)
{
	return set->add(val);
}

void *test(void *data) {
	val_t last = -1;
	val_t val = 0;
	int unext;
	thread_local int i= 0;

	benchthread_data_t *d = (benchthread_data_t *) data;

	/* Wait on barrier */
	barrier_cross(d->barrier);

	/* Is the first op an update? */
	unext = (rand_range_re(&d->seed, 100) - 1 < d->update);

	while (stop == 0) {
		if(i == 1000 ) {
			printf("Thread: %d\n", pthread_self());
			i= 0;
		}
		i++;
		if (unext) { // update

			if (last < 0) { // add
				val = rand_range_re(&d->seed, d->range);
				// if (sl_add(d->set, val, TRANSACTIONAL)) {
				if( d->set->add(val) ) {
					d->nb_added++;
					last = val;
				}
				d->nb_add++;

			} else { // remove

				if (d->alternate) { // alternate mode (default)

					// if (sl_remove(d->set, last, TRANSACTIONAL)) {
					if( d->set->remove(last) ) {
						d->nb_removed++;
					}
					last = -1;

				} else {

					// Random computation only in non-alternated cases
					val = rand_range_re(&d->seed, d->range);
					// Remove one random value
					// if (sl_remove(d->set, val, TRANSACTIONAL)) {
					if( d->set->remove(val)) {
						d->nb_removed++;
						// Repeat until successful, to avoid size variations
						last = -1;
					}

				}
				d->nb_remove++;
			}

		} else { // read

			if (d->alternate) {
				if (d->update == 0) {
					if (last < 0) {
						val = d->first;
						last = val;
					} else { // last >= 0
						val = rand_range_re(&d->seed, d->range);
						last = -1;
					}
				} else { // update != 0
					if (last < 0) {
						val = rand_range_re(&d->seed, d->range);
						//last = val;
					} else {
						val = last;
					}
				}
			} else
				val = rand_range_re(&d->seed, d->range);

			// if (sl_contains(d->set, val, TRANSACTIONAL))
			if( d->set->contains(val) )
				d->nb_found++;
			d->nb_contains++;

		}

		/* Is the next op an update? */
		if (d->effective) { // a failed remove/add is a read-only tx
			unext =
					((100 * (d->nb_added + d->nb_removed))
							< (d->update
									* (d->nb_add + d->nb_remove + d->nb_contains)));
		} else { // remove/add (even failed) is considered as an update
			unext = ((rand_range_re(&d->seed, 100) - 1) < d->update);
		}

	  }

	return NULL;
}


int main(int argc, char** argv){
	struct option long_options[] = {
	      // These options don't set a flag
	      {"help",                      no_argument,       NULL, 'h'},
		  {"list-type",                 required_argument, NULL, 'l'},
	      {"duration",                  required_argument, NULL, 'd'},
	      {"initial-size",              required_argument, NULL, 'i'},
	      {"thread-num",                required_argument, NULL, 't'},
	      {"range",                     required_argument, NULL, 'r'},
	      {"seed",                      required_argument, NULL, 'S'},
	      {"update-rate",               required_argument, NULL, 'u'},
	      {"unit-tx",                   required_argument, NULL, 'x'},
	      {NULL, 0, NULL, 0}
	    };

	SkipList *set;
	int i, c, size;
	val_t last = 0;
	val_t val = 0;
	unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read,
	  aborts_locked_write, aborts_validate_read, aborts_validate_write,
	  aborts_validate_commit, aborts_invalid_memory, max_retries;
	benchthread_data_t *data;
	pthread_t *threads;
	pthread_attr_t attr;
	barrier_t barrier;
	struct timeval start, end;
	struct timespec timeout;
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	long range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
	int unit_tx = DEFAULT_ELASTICITY;
	int alternate = DEFAULT_ALTERNATE;
	int effective = DEFAULT_EFFECTIVE;
	sigset_t block_set;
	int type= 0;

	while (1) {
		i = 0;
		c = getopt_long(argc, argv, "hAf:l:d:i:t:r:S:u:x:", long_options, &i);

		if (c == -1)
			break;

		if (c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;

		switch (c) {
		case 0:
			/* Flag is automatically set */
			break;
		case 'h':
			::std::cout << "intset -- STM stress test (skip list)\n\n";

			::std::cout << "Usage:\n";
			::std::cout << "  intset [options...]\n";
			::std::cout << "\n";
			::std::cout << "Options:\n";
			::std::cout << "  -h, --help\n";
			::std::cout << "        Print this message\n";
			::std::cout << "  -A, --Alternate\n";
			::std::cout
					<< "        Consecutive insert/remove target the same value\n";
			::std::cout << "  -l, --list-type TestList|LazySkipList|LockFreeSkipList\n";
			::std::cout << "  -f, --effective <int>\n";
			::std::cout
					<< "        update txs must effectively write (0=trial, 1=effective, default="
					<< DEFAULT_EFFECTIVE << ")\n";
			::std::cout << "  -d, --duration <int>\n";
			::std::cout
					<< "        Test duration in milliseconds (0=infinite, default="
					<< DEFAULT_DURATION << ")\n";
			::std::cout << "  -i, --initial-size <int>\n";
			::std::cout
					<< "        Number of elements to insert before test (default="
					<<
					DEFAULT_INITIAL << ")\n";
			::std::cout << "  -t, --thread-num <int>\n";
			::std::cout << "        Number of threads (default=" <<
			DEFAULT_NB_THREADS << ")\n";
			::std::cout << "  -r, --range <int>\n";
			::std::cout
					<< "        Range of integer values inserted in set (default="
					<< DEFAULT_RANGE << ")\n";
			::std::cout << "  -S, --seed <int>\n";
			::std::cout << "        RNG seed (0=time-based, default="
					<< DEFAULT_SEED << ")\n";
			::std::cout << "  -u, --update-rate <int>\n";
			::std::cout << "        Percentage of update transactions (default="
					<< DEFAULT_UPDATE << ")\n";
			::std::cout << "  -x, --unit-tx (default=1)\n";
			::std::cout << "        Use unit transactions\n";
			::std::cout << "        0 = non-protected,\n";
			::std::cout << "        1 = normal transaction,\n";
			::std::cout << "        2 = read unit-tx,\n";
			::std::cout << "        3 = read/add unit-tx,\n";
			::std::cout << "        4 = read/add/rem unit-tx,\n";
			::std::cout << "        5 = all recursive unit-tx,\n";
			::std::cout << "        6 = harris lock-free\n";
			exit(0);
		case 'A':
			alternate = 1;
			break;
		case 'l':
		{
			::std::string ltype(optarg);
			if( !ltype.compare("TestList") ) {
				type= 0;
			} else if( !ltype.compare("LazySkipList") ) {
				type= 1;
			} else if( !ltype.compare("LockFreeSkipList") ) {
				type= 2;
			}else {
				::std::cerr << "Unknown List-Type " << ltype << "\nExiting" << ::std::endl;
				exit(1);
			}
			break;
		}
		case 'f':
			effective = atoi(optarg);
			break;
		case 'd':
			duration = atoi(optarg);
			break;
		case 'i':
			initial = atoi(optarg);
			break;
		case 't':
			nb_threads = atoi(optarg);
			break;
		case 'r':
			range = atol(optarg);
			break;
		case 'S':
			seed = atoi(optarg);
			break;
		case 'u':
			update = atoi(optarg);
			break;
		case 'x':
			unit_tx = atoi(optarg);
			break;
		case '?':
			::std::cout << "Use -h or --help for help\n";
			exit(0);
		default:
			exit(1);
		}
	}

#ifdef MEM_MANAG
	manager= new MemoryManager();
#endif

	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(range > 0 && range >= initial);
	assert(update >= 0 && update <= 100);

	::std::cout << "Set type     : skip list\n";
	::std::cout << "Duration     : " << duration << ::std::endl;
	::std::cout << "Initial size : " << initial << ::std::endl;
	::std::cout << "Nb threads   : " << nb_threads << ::std::endl;
	::std::cout << "Value range  : " << range << ::std::endl;
	::std::cout << "Seed         : " << seed << ::std::endl;
	::std::cout << "Update rate  : " << update << ::std::endl;
	::std::cout << "Lock alg.    : " << unit_tx << ::std::endl;
	::std::cout << "Alternate    : " << alternate << ::std::endl;
	::std::cout << "Effective    : " << effective << ::std::endl;
	::std::cout << "Type sizes   : int=" << (int)sizeof(int) <<
			"/long=" << (int)sizeof(long) << "/ptr=" <<
			(int)sizeof(void *) << "/word=" << (int)sizeof(uintptr_t) << "\n";

	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;

	data = (benchthread_data_t *)xmalloc(nb_threads * sizeof(benchthread_data_t));
	threads = (pthread_t *)xmalloc(nb_threads * sizeof(pthread_t));

	if (seed == 0)
	  srand((int)time(0));
	else
	  srand(seed);

	levelmax = floor_log_2((unsigned int) initial);
	set = sl_set_new(type);
	stop = 0;

	global_seed = rand();
#ifdef TLS
	rng_seed = &global_seed;
#else /* ! TLS */
	if (pthread_key_create(&rng_seed_key, NULL) != 0) {
		fprintf(stderr, "Error creating thread local\n");
		exit(1);
	}
	pthread_setspecific(rng_seed_key, &global_seed);
#endif /* ! TLS */

	/* Populate set */
	::std::cout << "Adding " << initial << " entries to set\n";
	i = 0;
	while (i < initial) {
		val = rand_range_re(&global_seed, range);
		if (sl_add(set, val, 0)) {
			last = val;
			i++;
		}
	}
	size = sl_set_size(set);
	::std::cout << "Set size     : " << size << ::std::endl;
	::std::cout <<"Level max    : " << levelmax << ::std::endl;

	/* Access set from all threads */
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		::std::cout << "Creating thread " << i << ::std::endl;
		data[i].first = last;
		data[i].range = range;
		data[i].update = update;
		data[i].unit_tx = unit_tx;
		data[i].alternate = alternate;
		data[i].effective = effective;
		data[i].nb_add = 0;
		data[i].nb_added = 0;
		data[i].nb_remove = 0;
		data[i].nb_removed = 0;
		data[i].nb_contains = 0;
		data[i].nb_found = 0;
		data[i].nb_aborts = 0;
		data[i].nb_aborts_locked_read = 0;
		data[i].nb_aborts_locked_write = 0;
		data[i].nb_aborts_validate_read = 0;
		data[i].nb_aborts_validate_write = 0;
		data[i].nb_aborts_validate_commit = 0;
		data[i].nb_aborts_invalid_memory = 0;
		data[i].max_retries = 0;
		data[i].seed = rand();
		data[i].set = set;
		data[i].barrier = &barrier;
		if (pthread_create(&threads[i], &attr, test, (void *) (&data[i]))
				!= 0) {
			::std::cerr << "Error creating thread\n";
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);

	/* Start threads */
	barrier_cross(&barrier);

	printf("STARTING...\n");
	gettimeofday(&start, NULL);
	if (duration > 0) {
		nanosleep(&timeout, NULL);
	} else {
		sigemptyset(&block_set);
		sigsuspend(&block_set);
	}

#ifdef ICC
    stop = 1;
#else
    AO_store_full(&stop, 1);
#endif /* ICC */

    gettimeofday(&end, NULL);
    printf("STOPPING...\n");

    /* Wait for thread completion */
    for (i = 0; i < nb_threads; i++) {
      if (pthread_join(threads[i], NULL) != 0) {
	fprintf(stderr, "Error waiting for thread completion\n");
	exit(1);
      }
    }

    duration = (end.tv_sec * 1000 + end.tv_usec / 1000) -
      (start.tv_sec * 1000 + start.tv_usec / 1000);
    aborts = 0;
    aborts_locked_read = 0;
    aborts_locked_write = 0;
    aborts_validate_read = 0;
    aborts_validate_write = 0;
    aborts_validate_commit = 0;
    aborts_invalid_memory = 0;
    reads = 0;
    effreads = 0;
    updates = 0;
    effupds = 0;
    max_retries = 0;
    for (i = 0; i < nb_threads; i++) {
      printf("Thread %d\n", i);
      printf("  #add        : %lu\n", data[i].nb_add);
      printf("    #added    : %lu\n", data[i].nb_added);
      printf("  #remove     : %lu\n", data[i].nb_remove);
      printf("    #removed  : %lu\n", data[i].nb_removed);
      printf("  #contains   : %lu\n", data[i].nb_contains);
      printf("  #found      : %lu\n", data[i].nb_found);
      printf("  #aborts     : %lu\n", data[i].nb_aborts);
      printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
      printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
      printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
      printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
      printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
      printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
      printf("  Max retries : %lu\n", data[i].max_retries);
      aborts += data[i].nb_aborts;
      aborts_locked_read += data[i].nb_aborts_locked_read;
      aborts_locked_write += data[i].nb_aborts_locked_write;
      aborts_validate_read += data[i].nb_aborts_validate_read;
      aborts_validate_write += data[i].nb_aborts_validate_write;
      aborts_validate_commit += data[i].nb_aborts_validate_commit;
      aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
      reads += data[i].nb_contains;
      effreads += data[i].nb_contains +
	(data[i].nb_add - data[i].nb_added) +
	(data[i].nb_remove - data[i].nb_removed);
      updates += (data[i].nb_add + data[i].nb_remove);
      effupds += data[i].nb_removed + data[i].nb_added;
      size += data[i].nb_added - data[i].nb_removed;
      if (max_retries < data[i].max_retries)
	max_retries = data[i].max_retries;
    }
    printf("Set size      : %d (expected: %d)\n", sl_set_size(set), size);
    printf("Duration      : %d (ms)\n", duration);
    printf("#txs          : %lu (%f / s)\n", reads + updates,
	   (reads + updates) * 1000.0 / duration);

    printf("#read txs     : ");
    if (effective) {
      printf("%lu (%f / s)\n", effreads, effreads * 1000.0 / duration);
      printf("  #contains   : %lu (%f / s)\n", reads, reads * 1000.0 /
	     duration);
    } else printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);

    printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));

    printf("#update txs   : ");
    if (effective) {
      printf("%lu (%f / s)\n", effupds, effupds * 1000.0 / duration);
      printf("  #upd trials : %lu (%f / s)\n", updates, updates * 1000.0 /
	     duration);
    } else printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);

    printf("#aborts       : %lu (%f / s)\n", aborts, aborts * 1000.0 /
	   duration);
    printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read,
	   aborts_locked_read * 1000.0 / duration);
    printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write,
	   aborts_locked_write * 1000.0 / duration);
    printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read,
	   aborts_validate_read * 1000.0 / duration);
    printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write,
	   aborts_validate_write * 1000.0 / duration);
    printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit,
	   aborts_validate_commit * 1000.0 / duration);
    printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory,
	   aborts_invalid_memory * 1000.0 / duration);
    printf("Max retries   : %lu\n", max_retries);

    int added= 0;
    printf("#bench#addedT");
    for (i = 0; i < nb_threads; i++) {
    	printf(";%ld", data[i].nb_added);
    	added+= data[i].nb_added;
    }
    printf("\n#bench#added:%d\n", added);
	int removed = 0;
	printf("#bench#removedT");
	for (i = 0; i < nb_threads; i++) {
		printf(";%ld", data[i].nb_removed);
		removed += data[i].nb_removed;
	}
	printf("\n#bench#removed:%d\n", added);
	int contains = 0;
	printf("#bench#containsT");
	for (i = 0; i < nb_threads; i++) {
		printf(";%ld", data[i].nb_contains);
		contains += data[i].nb_added;
	}
	printf("\n#bench#contains:%d\n", contains);

    /* Delete set */
    sl_set_delete(set);

#ifndef TLS
    pthread_key_delete(rng_seed_key);
#endif /* ! TLS */

    free(threads);
    free(data);

	return 0;
}
