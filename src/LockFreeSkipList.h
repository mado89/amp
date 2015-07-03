#ifndef _LOCKFREESKIPLIST_H_
#define _LOCKFREESKIPLIST_H_

#include <list>
#include "SkipList.h"

#include <pthread.h>
#include <limits>
#include <atomic>

#include "MySPm.h"

class LockFreeSkipList: public SkipList {
private:
class LockFreeNode {
	public:
		pthread_mutex_t ilock;
		int item; // use as key... (sloppy)
		MySPm<LockFreeNode>* next;
		volatile bool marked = false;
		volatile bool fullylinked = false;
	private:
		int topLevel; // number of levels for node

	public:
		LockFreeNode(int x, int levels) : item(x) {

			// Init Lock. Set it to Reentrant behaviour
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&ilock, &attr);

			topLevel= levels;

			next= new MySPm<LockFreeNode>[levels+1]();
		}

		~LockFreeNode() {
			pthread_mutex_destroy(&ilock);
//			::std::cout << "~LockFreeNode " << this << ::std::endl;
			delete[] next;
		}

		void lock() {
			pthread_mutex_lock(&ilock);
		}

		void unlock() {
			pthread_mutex_lock (&ilock);
		}

		int height(){ return topLevel+1; }
};
typedef ::std::atomic<LockFreeNode*> LFNAp;

private:
	LockFreeNode* head;
	LockFreeNode* tail;

public:
	LockFreeSkipList(int levelmax) : SkipList(levelmax) {
		head= new LockFreeNode(::std::numeric_limits<int>::min(),levelmax);
		tail= new LockFreeNode(::std::numeric_limits<int>::max(),levelmax);

		for(int i= 0; i < head->height(); i++) {
			head->next[i].init(tail,false);
			// head->next[i].inc_ref();
		}
	}

	~LockFreeSkipList() {
//		::std::cout << "~LockFreeSkipList" << ::std::endl;
		/*for(int i= 0; i < head->height(); i++) {
			head->next[i].dec_ref();
		}*/
//		::std::cout << "Head next pointers decreased" << ::std::endl;
		delete head;
//		::std::cout << "Head deleted" << ::std::endl;
		delete tail;
//		::std::cout << "Tail deleted" << ::std::endl;
	}

private:
	int find(int key, LockFreeNode** preds, LockFreeNode** succs) {
		LockFreeNode* pred = NULL;
		LockFreeNode* curr = NULL;
		LockFreeNode* succ = NULL;
//		bool* marked;
//		bool mark;


	retry:
		while (true) {
			pred = head;
			for (int l = MAX_LEVEL; l >= 0; l--) {
				MySPmR<LockFreeNode>* dat= pred->next[l].get();
				curr= dat->addr;
				// curr = pred->next[l].get_pointer();
				while (true) {
//					succ = curr->next[l].get(&marked);
					dat= curr->next[l].get();
					succ= dat->addr;
//					succ = curr->next[l].get_pointer();
					//mark= curr->next[l].get_mark();
//					while(*marked) { // link out marked nodes
					while(dat->mark) { // link out marked nodes
						bool snip= pred->next[l].CASp(curr,succ,false,false);
						::std::cout << "In CAS loop " << snip << ::std::endl;
						if (!snip)
							goto retry;
						dat= pred->next[l].get();
						curr= dat->addr;
//						delete dat;
						dat= curr->next[l].get();
						succ= dat->addr;
//						curr = pred->next[l].get_pointer();
//						succ = curr->next[l].get(&marked);
//						succ = curr->next[l].get_pointer();
//						mark= curr->next[l].get_mark();
					}
					if( curr->item < key) {
						pred = curr;
						curr = succ;
					} else
						break;
			}
			preds[l] = pred;
			succs[l] = curr;
		}
		return ( curr->item == key);
	}
}

public:
	bool add(int x) {
		int k = random_level(MAX_LEVEL);
		int b = 0;

		LockFreeNode* preds[MAX_LEVEL+1];
		LockFreeNode* succs[MAX_LEVEL+1];

		while (true) {
			if (find(x, preds, succs))
				return false;
			else {
				LockFreeNode* newNode = new LockFreeNode(x, k);
				for (int l = b; l <= k; l++) {
					LockFreeNode* succ = succs[l];
					// newNode->next[l]->set(succ, false);
					newNode->next[l].init(succ, false);
				}

				LockFreeNode* pred = preds[b];
				LockFreeNode* succ = succs[b];

				//TODO: das kommt von hironobu
				newNode->next[b].init(succ, false);

				if (!pred->next[b].CASp(succ,newNode,false,false))
					continue;
				// remaining levels
				for (int l = b + 1; l <= k; l++) {
					while (true) {
						pred = preds[l];
						succ = succs[l];
//						::std::cout << "In Add-CAS loop " << ::std::endl;
						if (pred->next[l].CASp(succ, newNode, false, false))
							break;
						find(x, preds, succs);
					}
				}
				return true;
			}
		}
	}

	bool contains(int item) {
		int bottom = 0;
		LockFreeNode* pred = head;
		LockFreeNode* curr = NULL;
		LockFreeNode* succ = NULL;
		MySPmR<LockFreeNode>* dat;

		for (int l = MAX_LEVEL; l >= bottom; l--) {
			dat= pred->next[l].get();
			curr= dat->addr;
			while (true) {
//				succ = curr->next[l].get(&marked);
				dat= curr->next[l].get();
				succ= dat->addr;
//				succ = curr->next[l].get_pointer();
//				while (*marked) {
				int i= 0;
				while (dat->mark) {
					::std::cout << "In Contains marked " << i << ::std::endl;
					dat= curr->next[l].get();
					curr= dat->addr;
//					delete dat;
					dat= curr->next[l].get();
					succ= dat->addr;
//					curr = pred->next[l].get_pointer();
//					succ = curr->next[l].get(&marked);
//					succ = curr->next[l].get_pointer();
//					marked= curr->next[l].get_mark();
					i++;
				}
				if (curr->item < item) {
					pred = curr;
					curr = succ;
				} else
					break;
			}
		}
		return (curr->item == item);
	}

	bool remove(int x) {
		int b = 0;
		LockFreeNode* preds[MAX_LEVEL+1];
		LockFreeNode* succs[MAX_LEVEL+1];
		LockFreeNode* succ= NULL;
//		bool* marked;
		MySPmR<LockFreeNode>* dat;


		while (true) {
			if (!find(x, preds, succs))
				return false;
			else {
				// shortcut lists from k down to b+1
				LockFreeNode* remNode = succs[b];
				for (int l = remNode->height()-1; l > b; l--) {
					dat= remNode->next[l].get();
					succ= dat->addr;
//					succ = remNode->next[l].get_pointer();
					// succ = remNode->next[l].get(&marked);
//					bool mark= remNode->next[l].get_mark();
//					while (!(remNode->next[l].get_mark())) {
					while(!dat->mark) {
						remNode->next[l].CASp(succ, succ, false, true);
						// succ = remNode->next[l].get(&marked);
						dat= remNode->next[l].get();
						succ= dat->addr;
//						succ = remNode->next[l].get_pointer();
//						mark= remNode->next[l].get_mark();
					}
				}

				// level 0 list
//				succ = remNode->next[b].get(&marked);
				dat= remNode->next[b].get();
				succ= dat->addr;
//				succ = remNode->next[b].get_pointer();
//				bool marked= remNode->next[b].get_mark();
				while (true) {
					bool done = remNode->next[b].CASp(succ, succ, false, true);
//					succ = succs[b]->next[b].get(&marked);
					dat= succs[b]->next[b].get();
					succ= dat->addr;
//					succ = succs[b]->next[b].get_pointer();
//					marked= succs[b]->next[b].get_mark();
					if (done) {
						find(x, preds, succs); // clean up concurrent remove
//						delete remNode;
						return true;
//					} else if (*marked)
					} else if (dat->mark)
						return false;
				}
			}
		}
	}
};

#endif /*_LOCKFREESKIPLIST_H_*/
