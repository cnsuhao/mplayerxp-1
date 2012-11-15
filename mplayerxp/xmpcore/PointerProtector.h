#ifndef __POINTER_PROTECTOR_HPP_INCLUDED
#define __POINTER_PROTECTOR_HPP_INCLUDED
#include "mp_config.h"
#include <limits.h>
#include <stdlib.h>
#include <time.h>

typedef volatile any_t* __secure_ptr;

namespace xmpcore {
    class PointerProtector {
	public:
	    PointerProtector() { srand(time(0)); key=rand()%UINT_MAX; }
	    PointerProtector(unsigned _key):key(_key) {}

	    __secure_ptr protect(__secure_ptr& ptr) { ptr=reinterpret_cast<__secure_ptr>(reinterpret_cast<long>(ptr)^key); return ptr; }
	    __secure_ptr unprotect(__secure_ptr& ptr) { ptr=reinterpret_cast<__secure_ptr>(reinterpret_cast<long>(ptr)^key); return ptr; }
	private:
	    unsigned key;
    };
}

#endif
