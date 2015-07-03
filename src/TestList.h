#ifndef _TESTLIST_H_
#define _TESTLIST_H_

#include <list>
#include "SkipList.h"
#include <pthread.h>

class TestList: public SkipList {
private:
	::std::list<int>* internal;
	pthread_mutex_t int_mem_l;

public:
	TestList(int levelmax) : SkipList(levelmax) {
		internal= new ::std::list<int>();

		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&int_mem_l, NULL);
	}

	~TestList() {
		delete internal;
	}

	bool add(int x) {
		pthread_mutex_lock(&int_mem_l);
		::std::list<int>::iterator i;

		for (i = internal->begin(); i != internal->end() && (*i) < x; ++i);

		if( i == internal->end() ) {
			internal->push_back(x);
			pthread_mutex_unlock(&int_mem_l);
			return true;
		} else {
			if( *i != x ) {
				internal->insert(i,x);
				pthread_mutex_unlock(&int_mem_l);
				return true;
			}
		}

		pthread_mutex_unlock(&int_mem_l);
		return false;
	}

	bool contains(int x) {
		pthread_mutex_lock(&int_mem_l);
		::std::list<int>::iterator i;

		for (i = internal->begin(); i != internal->end() && (*i) != x; ++i);

		bool ret= i != internal->end();
		pthread_mutex_unlock(&int_mem_l);

		return ret;
	}

	bool remove(int x) {
		pthread_mutex_lock(&int_mem_l);
		internal->remove(x);
		pthread_mutex_unlock(&int_mem_l);
		return true;
	}
};

#endif /*_TESTLIST_H_*/
