/*
 * MemoryManager.h
 *
 *  Created on: Jul 3, 2015
 *      Author: martin
 */

#ifndef SRC_MEMORYMANAGER_H_
#define SRC_MEMORYMANAGER_H_

#include <map>
#include <atomic>
#include <pthread.h>

class MemoryManager{
private:
	::std::map<long,::std::atomic<int>*> int_mem;
	pthread_mutex_t int_mem_l;

public:
	MemoryManager() {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&int_mem_l, NULL);
	}

	~MemoryManager() {
		::std::map<long,::std::atomic<int>*>::iterator it= int_mem.begin();
		while( it != int_mem.end() ) {
			::std::cout << "Addr:" << it->first << ": " << *it->second << ::std::endl;
			it++;
		}
		pthread_mutex_destroy(&int_mem_l);
	}

	::std::atomic<int>* inc_ref(void* addr) {
		pthread_mutex_lock(&int_mem_l);
		::std::map<long,::std::atomic<int>*>::iterator it= int_mem.find((long)addr);
		if (it != int_mem.end()) {
			(*it->second)++;
			pthread_mutex_unlock(&int_mem_l);
			return it->second;
		} else {
			::std::cout << "Adding new node: " << (long) addr << ::std::endl;
			::std::atomic<int>* h = new ::std::atomic<int>(1);
			::std::pair<long,::std::atomic<int>*> val((long)addr, h);
			int_mem.insert(val);
			pthread_mutex_unlock(&int_mem_l);
			return h;
		}
	}

	void dec_ref(void* addr) {
		pthread_mutex_lock(&int_mem_l);
		::std::map<long,::std::atomic<int>*>::iterator it= int_mem.find((long)addr);
		(*it->second)--;
		if( (*it->second) == 0 ) {
			::std::atomic<int>* h= it->second;
			int_mem.erase(it);
			pthread_mutex_unlock(&int_mem_l);
			::std::cout << "Removing node: " << (long) addr << ::std::endl;
			free(addr);
			delete h;

		} else {
			pthread_mutex_unlock(&int_mem_l);
		}
	}

	void debugMem() {
		pthread_mutex_lock(&int_mem_l);
		::std::cout << "----Debug Memory ---- (begin)" << ::std::endl;
		::std::map<long,::std::atomic<int>*>::iterator it= int_mem.begin();
		while( it != int_mem.end() ) {
			::std::cout << "Addr:" << it->first << ": " << *it->second << ::std::endl;
			it++;
		}
		::std::cout << "----Debug Memory ---- (end)" << ::std::endl;
		pthread_mutex_unlock(&int_mem_l);
	}
};

MemoryManager* manager;

#endif /* SRC_MEMORYMANAGER_H_ */
