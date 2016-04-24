#pragma once

namespace taskloaf {
void _assert(const char* expression, const char* file, int line);
}
 
#ifdef TLASSERT
#define tlassert(EXPRESSION) ((EXPRESSION) ? (void)0 : _assert(#EXPRESSION, __FILE__, __LINE__))
#else
#define tlassert(EXPRESSION) ((void)0)
#endif
