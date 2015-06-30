#ifndef _MYSPM_H_
#define _MYSPM_H

#include<atomic>
#include<stddef.h>
#include<pthread.h>

template <class T> class MySPm{
private:
	::std::atomic<int>* c;
	T** i;
	// bool* m;
	bool m2;
	pthread_mutex_t l;
	pthread_mutexattr_t attr;

public:
	MySPm<T>() : c(NULL) {
		i= new T*;
//		m= new bool;
//		(*m)= false;
//		assert(m>0);
		m2= false;

		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&l, NULL);
	}

	MySPm<T>(T* t, bool mark=false) : c(1) {
		i= new T*;
		(*i)= t;
//		m= new bool;
//		(*m)= mark;
//		assert(m>0);
		m2= false;

		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&l, NULL);
	}

	~MySPm<T>() {
		// ::std::cout << "~MySPm " << (*c) << ::std::endl;
		if(c != NULL && *c == 0 ) {
			if(*i != NULL)
				delete *i;
		}
//		delete m;
		pthread_mutexattr_destroy(&attr);
	}

	void inc_ref() {
//		assert(m>0);
		(*c)++;
	}

	void dec_ref() {
//		assert(m>0);
		(*c)--;
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
		if( i == NULL )
			i= new T*;
		(*i) = val;
//		::std::cout << "Mark before " << m << " " << *m << ::std::endl;
//		(*m) = mark;
//		::std::cout << "Mark after " << m << " " << *m << ::std::endl;
		m2= mark;
		c= new ::std::atomic<int>(1);
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

#endif /* _MYSPM_H */
