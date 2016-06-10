#pragma once

namespace taskloaf {
void _assert(const char* expression, const char* file, int line);
}

#ifdef TASKLOAF_DEBUG
#define TL_IF_DEBUG(EXPRESSION,ALTERNATE) EXPRESSION
#else
#define TL_IF_DEBUG(EXPRESSION,ALTERNATE) ALTERNATE
#endif
 
#ifdef TASKLOAF_DEBUG
#define TLASSERT(EXPRESSION) ((EXPRESSION) ? (void)0 : _assert(#EXPRESSION, __FILE__, __LINE__))
#else
#define TLASSERT(EXPRESSION) ((void)0)
#endif
