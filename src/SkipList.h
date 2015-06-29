#ifndef _SKIPLIST_H_
#define _SKIPLIST_H_

class SkipList {
public:
	virtual bool add(int x) = 0;
	virtual bool contains(int x) = 0;
	virtual bool remove(int x) = 0;

	int size() {return 0;}
};

#endif /*_SKIPLIST_H_*/
