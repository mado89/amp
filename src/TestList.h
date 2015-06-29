#ifndef _TESTLIST_H_
#define _TESTLIST_H_

#include <list>
#include "SkipList.h"

class TestList: public SkipList {
private:
	::std::list<int>* internal;

public:
	TestList() {
		internal= new ::std::list<int>();
	}

	~TestList() {
		delete internal;
	}

	bool add(int x) {
		::std::list<int>::iterator i;

		for (i = internal->begin(); i != internal->end() && (*i) < x; ++i);

		if( i == internal->end() ) {
			internal->push_back(x);
			return true;
		} else {
			if( *i != x ) {
				internal->insert(i,x);
				return true;
			}
		}

		return false;
	}

	bool contains(int x) {
		::std::list<int>::iterator i;

		for (i = internal->begin(); i != internal->end() && (*i) != x; ++i);

		return i != internal->end();
	}

	bool remove(int x) {
			internal->remove(x);
			return true;
	}
};

#endif /*_TESTLIST_H_*/
