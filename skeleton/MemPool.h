#ifndef _MPOOL_
#define _MPOOL_

#include "headers.h"

class MemPool {
	static MemPool *link; static bool nfirst_i;
	uint size;
	typedef struct Store {
		Store *next;
		size_t mov;
	} Store;
	Store *head;
	Store *prev;
	MemPool() {}
	~MemPool() {CleanUp();}
public:
	static MemPool *CreateObj() {
		if(!link) link = new MemPool();
		return link;
	};
	void NewPool(uint s) {if(nfirst_i) this->~MemPool(); head = 0; size = s; nfirst_i = 1;}
	char *allocate(size_t);
	void CleanUp();
};

extern MemPool *Pool;

#endif
