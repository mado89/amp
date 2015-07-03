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

#ifdef MEM_MANAG
extern MemoryManager* manager;
#endif

SkipList* sl_set_new(int type)
{
	SkipList *set;

	if( type == 0) {
		::std::cout << "Using TestList" << ::std::endl;
		set= new TestList(levelmax);
	}
	else if( type == 1 ){
		::std::cout << "Using LazySkipList" << ::std::endl;
		set= new LazySkipList(levelmax);
	}
	else if( type == 2 ){
		::std::cout << "Using LockFreeSkipList" << ::std::endl;
		set= new LockFreeSkipList(levelmax);
	}
	else
		set= NULL;

	return set;
}

void sl_set_delete(SkipList *set) {
	delete set;
}

typedef struct tdata {
  val_t first;
  long range;
  unsigned long nb_add;
  unsigned long nb_added;
  unsigned long nb_remove;
  unsigned long nb_removed;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long nb_aborts;
  unsigned long nb_aborts_locked_read;
  unsigned long nb_aborts_locked_write;
  unsigned long nb_aborts_validate_read;
  unsigned long nb_aborts_validate_write;
  unsigned long nb_aborts_validate_commit;
  unsigned long nb_aborts_invalid_memory;
  unsigned long max_retries;
  unsigned int seed;
  SkipList *set;
  barrier_t *barrier;
} tdata_t;

int sl_set_size(SkipList *set)
{
	return set->size();
}

void *test(void *data) {
	int first;

	tdata_t *d = (tdata_t *) data;

	/* Wait on barrier */
	barrier_cross(d->barrier);

	first= d->first;
	while(first < d->range){
		d->nb_add++;
		// ::std::cout << pthread_self() << ": Adding " << first << " " << d->range << ::std::endl;
		if( d->set->add(first) ){
			// ::std::cout << "Added" << first << ::std::endl;
			first++;
			d->nb_added++;
		} /*else {
			::std::cout << "Failed" << first << ::std::endl;
		}*/
	}

	return NULL;
}

void *test_remove(void *data) {
	int first;

	tdata_t *d = (tdata_t *) data;

	/* Wait on barrier */
	barrier_cross(d->barrier);

	first= d->first;
	while(first < d->range){
		d->nb_remove++;
//		::std::cout << pthread_self() << ": Remove " << first << " " << d->range << ::std::endl;

		if( d->set->remove(first) ){
			// ::std::cout << "Removed" << first << ::std::endl;
			first++;
			d->nb_removed++;
		} /*else {
			::std::cout << "Failed" << first << ::std::endl;
		}*/
	}

	return NULL;
}

void *test_contains(void *data) {
	int first;

	tdata_t *d = (tdata_t *) data;

	/* Wait on barrier */
	barrier_cross(d->barrier);

	first= d->first;
	while(first < d->range){
		d->nb_contains++;
		// ::std::cout << pthread_self() << ": Contains " << first << " " << d->range << ::std::endl;

		if( d->set->contains(first) ){
			// ::std::cout << "Found" << first << ::std::endl;
			first++;
			d->nb_found++;
		} /*else {
			::std::cout << "Failed" << first << ::std::endl;
		}*/
	}

	return NULL;
}

int main(int argc, char** argv){
	struct option long_options[] = {
	      // These options don't set a flag
	      {"help",                      no_argument,       NULL, 'h'},
		  {"list-type",                 required_argument, NULL, 'l'},
		  {"initial-size",              required_argument, NULL, 'i'},
		  {"thread-num",                required_argument, NULL, 't'},
	      {"range",                     required_argument, NULL, 'r'},
	      {NULL, 0, NULL, 0}
	    };

	SkipList *set;
	int i, c, size;
	unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read,
		  aborts_locked_write, aborts_validate_read, aborts_validate_write,
		  aborts_validate_commit, aborts_invalid_memory, max_retries;
	pthread_t *threads;
	pthread_attr_t attr;
	barrier_t barrier;
	struct timeval start, end;
	int duration = DEFAULT_DURATION;
	int nb_threads = DEFAULT_NB_THREADS;
	long range = DEFAULT_RANGE;
	int initial = DEFAULT_INITIAL;
	int type= 0;

	while (1) {
		i = 0;
		c = getopt_long(argc, argv, "h:l:i:t:r:", long_options, &i);

		if (c == -1)
			break;

		if (c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;

		switch (c) {
		case 0:
			/* Flag is automatically set */
			break;
		case 'h':
			::std::cout << "intset -- Simple correctness test (skip list)\n\n";

			::std::cout << "Usage:\n";
			::std::cout << "  test_correct_simple [options...]\n";
			::std::cout << "\n";
			::std::cout << "Options:\n";
			::std::cout << "  -h, --help\n";
			::std::cout << "        Print this message\n";
			::std::cout << "  -l, --list-type TestList|LazySkipList|LockFreeSkipList\n";
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
			exit(0);
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
		case 'i':
			initial = atoi(optarg);
			break;
		case 't':
			nb_threads = atoi(optarg);
			break;
		case 'r':
			range = atol(optarg);
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
	assert(nb_threads > 0);
	assert(range > 0 && range >= initial);

	::std::cout << "Set type     : skip list\n";
	::std::cout << "Nb threads   : " << nb_threads << ::std::endl;
	::std::cout << "Value range  : " << range << ::std::endl;
	::std::cout << "Type sizes   : int=" << (int)sizeof(int) <<
			"/long=" << (int)sizeof(long) << "/ptr=" <<
			(int)sizeof(void *) << "/word=" << (int)sizeof(uintptr_t) << "\n";

	levelmax = floor_log_2((unsigned int) range);

	::std::cout << "Constructing: ";
	set = sl_set_new(type);
	::std::cout << "done" << ::std::endl;

	::std::cout << "Adding " << initial << " entries to set\n";
	i = 0;
	while (i < initial) {
		// ::std::cout << "Add " << set << " " << i << ::std::endl;
		if (set->add(i)) {
			i++;
		}
	}
	size = sl_set_size(set);
	::std::cout << "Set size     : " << size << ::std::endl;
	::std::cout << "Level max    : " << levelmax << ::std::endl;

	/* Access set from all threads */
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	tdata_t* data;
	data = (tdata_t *)malloc(nb_threads * sizeof(tdata_t));
	threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t));
	int h;

	h= (range - initial);
	h/= nb_threads;

	for (i = 0; i < nb_threads; i++) {

		data[i].first= initial + h*i;
		data[i].range = initial + h*(i+1);

		::std::cout << "Creating thread " << i << ::std::endl;
		::std::cout << "first " << data[i].first << " " << data[i].range << ::std::endl;

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

		if (pthread_create(&threads[i], &attr, test, (void *) (&data[i]))!= 0) {
			::std::cerr << "Error creating thread\n";
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);

	/* Start threads */
	barrier_cross(&barrier);

	printf("STARTING...\n");
	gettimeofday(&start, NULL);

    printf("STOPPING...\n");

    /* Wait for thread completion */
	for (i = 0; i < nb_threads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}

	::std::cout << "Numbers Added" << ::std::endl;

	for (i = 0; i < nb_threads; i++) {
		::std::cout << "Starting thread " << i << ::std::endl;

		if (pthread_create(&threads[i], &attr, test_contains, (void *) (&data[i]))!= 0) {
			::std::cerr << "Error creating thread\n";
			exit(1);
		}
	}

	/* Start threads */
	barrier_cross(&barrier);

	printf("STOPPING...\n");

	/* Wait for thread completion */
	for (i = 0; i < nb_threads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}

	::std::cout << "Numbers Tested" << ::std::endl;

		for (i = 0; i < nb_threads; i++) {
			::std::cout << "Starting thread " << i << ::std::endl;

			if (pthread_create(&threads[i], &attr, test_remove, (void *) (&data[i]))!= 0) {
				::std::cerr << "Error creating thread\n";
				exit(1);
			}
		}

		/* Start threads */
		barrier_cross(&barrier);

		printf("STOPPING...\n");

		/* Wait for thread completion */
		for (i = 0; i < nb_threads; i++) {
			if (pthread_join(threads[i], NULL) != 0) {
				fprintf(stderr, "Error waiting for thread completion\n");
				exit(1);
			}
		}

    gettimeofday(&end, NULL);

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
    size= 0;
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
    printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);

    printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));

    printf("#update txs   : ");
    printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);

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

#ifdef MEM_MANAG
	delete manager;
#endif

	return 0;
}
