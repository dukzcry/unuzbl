#include "MemPool.h"
#include "headers.h"

#include <pthread.h> // Threads model (already in headers.h, but sometimes
		     // linking fails 

bool MemPool::nfirst_i = 0;
MemPool *MemPool::link = NULL;
MemPool *Pool = MemPool::CreateObj(); // Singleton
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void MemPool::CleanUp()
{
pthread_mutex_lock(&lock);
	Store *nextptr = head;
	for(; nextptr; nextptr = head) {
		head = head->next;
		delete [] nextptr; // But if something goes bad, 
				   // there is no way to return mem to OS :(
	}
pthread_mutex_unlock(&lock);
}

char *MemPool::allocate(size_t s)
{
pthread_mutex_lock(&lock);
	Store *st, **ptr;
	if(s >= size) {
		ptr = &head;
		while(*ptr) ptr = &(*ptr)->next;
		*ptr = st = (Store *) (new char [sizeof(Store) + s]);
		if(!st) std::cout << "Memory allocation failed" << std::endl;
		st->next = NULL; st->mov = size;
		prev = st;
		return (char *)(st + 1);
	}
pthread_mutex_unlock(&lock);
	st = head; while(st && st->mov + s > size) st = st->next;
	if(st) {
		char *ptr = st->mov + (char *)(st + 1);
		st->mov += s; prev = st; return ptr;
	}
pthread_mutex_lock(&lock);
	st = (Store *) (new char [sizeof(Store) + size]);
pthread_mutex_unlock(&lock);
	if(!st) std::cout << "Memory allocation failed" << std::endl;
	st->next = head; head = st; st->mov = s; prev = st; return (char *)(st + 1);
}
