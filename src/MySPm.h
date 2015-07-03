#ifndef _MYSPM_H_
#define _MYSPM_H

#include <atomic>
#include <stddef.h>
#include <pthread.h>
#include <map>

template <typename T> class MySPm{
private:
	::std::atomic<int>* c;
	T** i;
	// bool* m;
	bool m2;
	pthread_mutex_t l;

	typedef typename ::std::map<T*,::std::atomic<int>*> mymap;
	typedef typename MySPm<T>::mymap::iterator iterator;

	static ::std::map<T*,::std::atomic<int>*> int_mem;
	static pthread_mutex_t int_mem_l;

public:
	MySPm<T>() : c(NULL) {
		i= new T*;
//		m= new bool;
//		(*m)= false;
//		assert(m>0);
		m2= false;

		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&l, NULL);
	}

	MySPm<T>(T* t, bool mark=false) : c(NULL) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&l, NULL);
		pthread_mutexattr_destroy(&attr);

		init(t, mark);
	}

	~MySPm<T>() {
		// ::std::cout << "~MySPm " << (*c) << ::std::endl;
		if(c != NULL && *c == 0 ) {
			if(*i != NULL)
				delete *i;
		}
		delete i;
//		delete m;
	}

	void inc_ref() {
//		assert(m>0);
		(*c)++;
	}

	static ::std::atomic<int>* inc_ref(T* addr) {
		iterator it;
		it= MySPm<T>::int_mem.find(addr);
		if( it != MySPm<T>::int_mem.end() ) {
			(*it->second)++;
			return it->second;
		} else {
			::std::atomic<int>* h= new ::std::atomic<int>(1);
			::std::pair<T*,::std::atomic<int>*> val(addr,h);
			MySPm<T>::int_mem.insert(it, val);
			return h;
		}
	}

	static void debugMem() {

	}

	void dec_ref() {
//		assert(m>0);
		(*c)--;
	}

	static void dec_ref(T** addr) {
		iterator it;
		it= MySPm<T>::int_mem.find(addr);
		(*it->second)--;
		if( (*it->second) == 0 ) {
			delete *addr;
			MySPm<T>::int_mem.erase(it);
		}
	}

	/*T* get(bool **marked) {
		::std::cout << m << " " << i << ::std::endl;
		*marked= m;
		return *i;
	}*/

	T* get_pointer() {
//		assert(m>0);
		return *i;
	}

	bool get_mark() {
//		assert(m>0);
//		return (*m);
		return m2;
	}

	void set_mark(bool mark) {
		pthread_mutex_lock(&l);
//		*m= mark;
		m2= mark;
		pthread_mutex_unlock(&l);
//		assert(m>0);
	}

	void init(T* val, bool mark) {
		pthread_mutex_lock(&l);
		if( i != NULL )
			delete i;
		i= new T*;
		(*i) = val;
//		::std::cout << "Mark before " << m << " " << *m << ::std::endl;
//		(*m) = mark;
//		::std::cout << "Mark after " << m << " " << *m << ::std::endl;
		m2= mark;
		//int_mem.

		c= inc_ref(val);
//		assert(m>0);
		pthread_mutex_unlock(&l);
	}

	/**
	 * CAS operation for pointer and mark
	 */
	bool CASp(T* exp, T* newp, bool expb, bool newb) {
		bool ret= false;

//		assert(m>0);
		pthread_mutex_lock(&l);
//		if( *i == exp && (*m) == expb ) {
//		if( debug )
//			::std::cout << "CAS " << *i << " " << exp << " " << newp << " " << m2 << " " <<expb << " " << newb << ::std::endl;

		if( *i == exp && m2 == expb ) {
			(*i)= newp;
//			(*m)= newb;
//			assert(m>0);
			m2= newb;
			ret= true;
		}
		pthread_mutex_unlock(&l);

		return ret;
	}
};

template<typename T> ::std::map<T*,::std::atomic<int>*> MySPm<T>::int_mem;
template<typename T> pthread_mutex_t MySPm<T>::int_mem_l;

#endif /* _MYSPM_H */
