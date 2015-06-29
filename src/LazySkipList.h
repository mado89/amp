#ifndef _LAZYSKIPLIST_H_
#define _LAZYSKIPLIST_H_

#include <list>
#include "SkipList.h"

#include <pthread.h>
#include <limits>

class LazySkipList: public SkipList {
private:
class LazyNode {
	public:
		pthread_mutex_t ilock;
		int item; // use as key... (sloppy)
		LazyNode** next;
		volatile bool marked = false;
		volatile bool fullylinked = false;
	private:
		int topLevel; // number of levels for node

	public:
		LazyNode(int x, int levels) : item(x), next((LazyNode**) new LazyNode*[levels+1]) {

			// Init Lock. Set it to Reentrant behaviour
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&ilock, &attr);

			topLevel= levels;
		}

		~LazyNode() {
			pthread_mutex_destroy(&ilock);
			for(int i= 0; i < topLevel+1; i++)
				delete next[i];
		}

		void lock() {
			pthread_mutex_lock(&ilock);
		}

		void unlock() {
			pthread_mutex_lock (&ilock);
		}

		int height(){ return topLevel+1; }
};

private:
	LazyNode* head;
	LazyNode* tail;
public:
	LazySkipList(int levelmax) : SkipList(levelmax) {
		head= new LazyNode(::std::numeric_limits<int>::min(),levelmax);
		tail= new LazyNode(::std::numeric_limits<int>::max(),levelmax);

		for(int i= 0; i < head->height(); i++) {
			head->next[i]= tail;
		}
	}

	~LazySkipList() {

	}

private:
	int find(int key, LazyNode** preds, LazyNode** succs) {
		int foundLevel= -1;
		LazyNode* pred= head;

		for(int l = MAX_LEVEL; l >= 0; l--) {
			LazyNode* curr = pred->next[l];
			while (key > curr->item) {
				pred = curr;
				curr = curr->next[l];
			}
			if (foundLevel == -1 && key == curr->item)
				foundLevel = l;
			preds[l] = pred;
			succs[l] = curr;
		}
		return foundLevel;
	}

public:
	bool add(int x) {
		int k = random_level(MAX_LEVEL);

		LazyNode* preds[MAX_LEVEL+1];
		LazyNode* succs[MAX_LEVEL+1];

		while (true) {
			int f = find(x, preds, succs);
			if (f >= 0) {
				LazyNode* Found = succs[f];
				if (!Found->marked) {
					while (!Found->fullylinked) {
						// spin
					}
					return false;
				}
				continue;
			}
			// not in skiplist, try to add

			int highlock = -1; // highest level locked

			LazyNode* pred;
			LazyNode* succ;

			bool valid = true;

			for (int level = 0; valid && (level <= k); level++) { // validate
				pred = preds[level];
				succ = succs[level];
				pred->lock();
				highlock = level;
				valid = !pred->marked && !succ->marked && (pred->next[level] == succ);
			}
			if (!valid) {
				for (int l = 0; l <= highlock; l++) {
					preds[l]->unlock();
				}
				continue; // failed, retry
			}

			LazyNode* newNode = new LazyNode(x, k); // allocate new
			for (int level = 0; level <= k; level++) {
				newNode->next[level] = succs[level];
				preds[level]->next[level] = newNode;
			}
			newNode->fullylinked = true;

			for(int l= 0; l <= highlock; l++){
				preds[l]->unlock();
			}

			return true;
		}
	}

	bool contains(int item) {
		LazyNode* preds[MAX_LEVEL+1];
		LazyNode* succs[MAX_LEVEL+1];

		int found_level = find(item,preds,succs);

		return (found_level >= 0 && succs[found_level]->fullylinked
				&& !succs[found_level]->marked);
	}

	bool remove(int x) {
		LazyNode* victim = NULL;
		bool marked = false;
		int k = -1;

		LazyNode* preds[MAX_LEVEL+1];
		LazyNode* succs[MAX_LEVEL+1];

		while (true) {
			int f = find(x, preds, succs);
			if (f >= 0)
				victim = succs[f];
			if (marked || (f >= 0 && victim->fullylinked && victim->height() == f
							&& !victim->marked)) {
				if (!marked) {
					k = victim->height();
					victim->lock();
					if (victim->marked) {
						victim->unlock();
						return false;
					}
					victim->marked = true;
					marked = true;
				}
				// try to remove
				int highlock = -1;
				LazyNode* pred;

				bool valid = true;
				for (int level=0; valid&&(level<=k); level++) {
					pred = preds[level];
					pred->lock();
					highlock = level;
					valid = !pred->marked&&pred->next[level]==victim;
				}
				if (!valid){
					for (int l = 0; l <= highlock; l++)
						preds[l]->unlock();
					continue;
				}
				for (int l=k; l>=0; l--)
					preds[l]->next[l] = victim->next[l];
				victim->unlock();
				for (int l = 0; l <= highlock; l++)
					preds[l]->unlock();
				return true;
			}
			else {
				return false;
			}
		}
	}
};

#endif /*_LAZYSKIPLIST_H_*/
