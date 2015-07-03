#ifndef _LAZYSKIPLIST_H_
#define _LAZYSKIPLIST_H_

#include <list>
#include "SkipList.h"

#include <pthread.h>
#include <limits>

#ifdef MEM_MANAG
#include "MemoryManager.h"
extern MemoryManager* manager;
#endif

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
#ifdef DEBUG
			::std::cout << this << " LazyNode::LazyNode(" << x << "," << levels << ")" << ::std::endl;
			::std::cout << "Next: " << next << ::std::endl;
#endif
			// Init Lock. Set it to Reentrant behaviour
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&ilock, &attr);

			topLevel= levels;

			for(int i= 0; i <= levels; i++)
				next[i]= NULL;
		}

		~LazyNode() {
#ifdef DEBUG
			::std::cout << this << " LazyNode::~LazyNode" << ::std::endl;
#endif
			pthread_mutex_destroy(&ilock);
			/*for(int i= 0; i < topLevel+1; i++)
				delete next[i];*/
			delete[] next;
		}

		void lock() {
			// ::std::cout << this << " lock" << pthread_self() << " " << ::std::endl;
			pthread_mutex_lock(&ilock);
		}

		void unlock() {
			// ::std::cout << this << " unlock" << ::std::endl;
			pthread_mutex_unlock (&ilock);
		}

		int height(){ return topLevel; }
//#ifdef MEM_MANAG
		friend void destroy(void*);
//#endif
};

//#ifdef MEM_MANAG
void destroy(void* obj) {
#ifdef DEBUG
			::std::cout << obj << " LazyNode::destroy" << ::std::endl;
#endif
	pthread_mutex_destroy(&((LazyNode*)obj)->ilock);
	for(int i= 0; i < ((LazyNode*)obj)->topLevel+1; i++) {
#ifdef MEM_MANAG
		manager->dec_ref(((LazyNode*)obj)->next[i]);
#endif
		//delete next[i];
	}
#ifdef DEBUG
			::std::cout << "LazyNode::destroy delete" << ((LazyNode*)obj)->next << ::std::endl;
#endif
	delete[] ((LazyNode*)obj)->next;
}
//#endif

class LazySkipList: public SkipList {
private:
	LazyNode* head;
	LazyNode* tail;
public:
	LazySkipList(int levelmax) : SkipList(levelmax) {
#ifdef DEBUG
		::std::cout << this << " LazySkipList::LazySkipList(" << levelmax << ")" << ::std::endl;
#endif
		
		LazyNode* a= (LazyNode*) malloc(sizeof(LazyNode));
		head= new (a) LazyNode(::std::numeric_limits<int>::min(),levelmax);

		LazyNode* b= (LazyNode*) malloc(sizeof(LazyNode));
		tail= new (b) LazyNode(::std::numeric_limits<int>::max(),levelmax);
		
#ifdef MEM_MANAG
		manager->inc_ref(head);
		manager->inc_ref(tail);
		manager->debugMem();
#endif

		for(int i= 0; i <= head->height(); i++) {
#ifdef MEM_MANAG
			manager->inc_ref(tail);
#endif
			head->next[i]= tail;
		}
	}

	~LazySkipList() {
		head->~LazyNode();
		tail->~LazyNode();
		free(head);
		free(tail);
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
	void debugList() {
		for(int l = MAX_LEVEL; l >= 0; l--) {
			::std::cout << "Level: " << l << ::std::endl;
			::std::cout << head << "/" << head->item;
			LazyNode* cur= head->next[l];
			while(cur) {
				::std::cout << " " << cur << "/" << cur->item;
				cur= cur->next[l];
			}
			::std::cout << ::std::endl;
		}
	}

	bool add(int x) {
		int k = random_level(MAX_LEVEL);

		LazyNode* preds[MAX_LEVEL+1];
		LazyNode* succs[MAX_LEVEL+1];

		while (true) {
			// ::std::cout << "Attempt find" << ::std::endl;
			int f = find(x, preds, succs);
			// ::std::cout  << pthread_self()<< ": Attempt find ended " << f << ::std::endl;
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

			// ::std::cout << "Attempt add" << ::std::endl;

			int highlock = -1; // highest level locked

			LazyNode* pred;
			LazyNode* succ;

			bool valid = true;

			for (int level = 0; valid && (level <= k); level++) { // validate
				// ::std::cout << level <<" " << k << " " << MAX_LEVEL << ::std::endl;
				pred = preds[level];
				succ = succs[level];
				// ::std::cout << pred << ::std::endl;
				pred->lock();
				highlock = level;
				valid = !pred->marked && !succ->marked && (pred->next[level] == succ);
			}
			// ::std::cout << "Validation done" << ::std::endl;
			if (!valid) {
				for (int l = 0; l <= highlock; l++) {
					preds[l]->unlock();
				}
				continue; // failed, retry
			}

			LazyNode* a= (LazyNode*) malloc(sizeof(LazyNode));
			LazyNode* newNode = new (a) LazyNode(x, k); // allocate new
			for (int level = 0; level <= k; level++) {
				newNode->next[level] = succs[level];
				preds[level]->next[level] = newNode;
#ifdef MEM_MANAG
				manager->inc_ref(newNode);
#endif
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

		// ::std::cout << "remove " << x << ::std::endl;

		while (true) {
			int f = find(x, preds, succs);
			// ::std::cout << "f " << f << ::std::endl;
			if (f >= 0)
				victim = succs[f];
			// ::std::cout << "f" << f << " marked? " << marked << " f>=0" << (f>=0) << " victim->fullylinked" << victim->fullylinked << " victim->height" << victim->height() << " victim->marked" << victim->marked << ::std::endl;
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
#ifdef MEM_MANAG
				//victim is needed later for unlocking
				manager->inc_ref(victim);
#endif
				for (int l=k; l>=0; l--) {
#ifdef MEM_MANAG
					manager->dec_ref(preds[l]->next[l],&destroy);
#endif
					preds[l]->next[l] = victim->next[l];
				}
				victim->unlock();
				for (int l = 0; l <= highlock; l++)
					preds[l]->unlock();
#ifdef MEM_MANAG
				manager->dec_ref(victim,&destroy);
#endif
/*#else
				destroy(victim);
				free(victim);
#endif*/
				return true;
			}
			else {
				return false;
			}
		}
	}
};

#endif /*_LAZYSKIPLIST_H_*/
