//
// General miscellaneous stuff that belongs nowhere else.
//
// $Header$
// Written by: Jeff Bilmes
//             bilmes@icsi.berkeley.edu


#ifndef GENERAL_H
#define GENERAL_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "machine-dependent.h"

#define VCID(x) static char * version_control_id = x; static char *___tmp___ = version_control_id;

char *copyToNewStr(const char *const str);


// a general swapping routine.
template <class T>
void genSwap(T& v1, T& v2) 
{
   T tmp = v1;
   v1 = v2;
   v2 = tmp;
}

#define CSWT_EMPTY_TAG (~0)
// Copies input over to result and if 
// input has any occurence of '%d' in it, replace it with
// the string version of the integer tag. Assume
// result is plenty big.
void copyStringWithTag(char *result,char *input,
		       const int tag, const int maxLen);


unsigned long fsize(FILE*stream);
unsigned long fsize(const char* const filename);




#endif
