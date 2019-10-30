#ifndef _SPOS_LAB1_DEMOFUNCS
#define _SPOS_LAB1_DEMOFUNCS

#include <stdbool.h>

#define TYPE_and	bool
#define TYPE_or 	bool
#define TYPE_imul	int
#define TYPE_fmul	double
#define TYPE_imin	unsigned int
#define TYPE_fmin	double

#define DECLARE_FUNCS(op)			\
	extern TYPE_##op f_func_##op(int x);	\
	extern TYPE_##op g_func_##op(int x) 

DECLARE_FUNCS(and);
DECLARE_FUNCS(or);
DECLARE_FUNCS(imul);
DECLARE_FUNCS(fmul);
DECLARE_FUNCS(imin);
DECLARE_FUNCS(fmin);

#endif // _SPOS_LAB1_DEMOFUNCS
