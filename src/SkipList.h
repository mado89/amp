#ifndef _SKIPLIST_H_
#define _SKIPLIST_H_

/*
 * Taken from skiplist-lock.c of synchrobench, Author(s): Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might
 * be too high for given values of range and initial.
 */
inline long random_level(long r) {
	int m = RAND_MAX;
	long d, v = 0;

	do {
		d = (m > r ? r : m);
		v += 1 + (long)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}

class SkipList {
public:
	SkipList(int levelmax) : MAX_LEVEL(levelmax){}
	virtual ~SkipList() {}

public:
	int MAX_LEVEL;
	virtual bool add(int x) = 0;
	virtual bool contains(int x) = 0;
	virtual bool remove(int x) = 0;

	int size() {return 0;}
};

#endif /*_SKIPLIST_H_*/
