/*
 * GMTK_PackCliqueValue.h
 *   PackCliqueValue:
 *
 *     Class to support packing/unpacking of a vector of unsigned into
 *     a packed machine word representation.  It uses an array, the
 *     first portion for those elements that fit within a word
 *     boundary and the second portion for those that cross word
 *     boundaries.  This will obtain better branch prediction behavior
 *     (since otherwise, it'll be harder for the processor to
 *     accurately predict branch outcomes). The algorithm tries to
 *     find a good allocation of vals to minimize those that cross
 *     word boundaries (since that involves more code per unit).
 *
 * Written by Jeff Bilmes <bilmes@ee.washington.edu>
 *
 * Copyright (C) 2003 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 * See COPYING or http://opensource.org/licenses/OSL-3.0
 *
 *
 *
 * $Header$
 *
 */

#ifndef GMTK_PACK_CLIQUE_VALUE_H
#define GMTK_PACK_CLIQUE_VALUE_H

#include <vector>
#include <string>
#include <map>

#include <stdio.h>
#include <stdlib.h>


#include "debug.h"

// class mention for forward references.
class Partition;
class GMTemplate;
class RV;

class PackCliqueValue {

  friend class MaxClique;

#ifdef MAIN
  friend int main(int,char**);
#endif

  unsigned numUnsignedInPackedVector;

  // Ideally, should be const.
  // const unsigned unpackedVectorLength;
  unsigned unpackedVectorLength;

  // Structure to hold information in going
  // from a packed word to an unpacked word.
  struct ValLocator {
    // the starting word in a packed vector
    unsigned start;
    // the amount to right-shift the packed word
    // to get an unpacked word
    unsigned startRightShift;
    // location in unpacked array
    unsigned loc;

    // the mask in this first word
    unsigned startMask;

    // if we overlap to next machine word, the mask
    // in that next word, these bits are to be or'd in
    // the higher order bits of the unpacked word.
    unsigned nextMask;
    // the amount to left shift these masked bits to
    // form the high order bits of the unpacked word.
    unsigned nextLeftShift;

    // bool operator<(const ValLocator&o) { return start < o.start; }
    
  };

  sArray< ValLocator> valLocators;
  sArray< unsigned> valBits;
  // pointer to end of case where there is no word boundary.
  ValLocator *member_vl_nwb_endp;
  // pointer to end of array.
  ValLocator *member_vl_endp;


  // Total number of bits in this packed clique, meaning the total
  // number of bits that are required for each packed value. This
  // will be <= packedLen()*sizeof(unsigned)*8.
  unsigned totalNumBits;


  // initialize
  void init(const unsigned *const cards,bool useNaive = false);

public:

  PackCliqueValue(vector<RV*>& nodes);

  PackCliqueValue(const unsigned len, const unsigned *const cards, bool useNaive = false);

  // create an empty one for re-construction later
  PackCliqueValue() : numUnsignedInPackedVector(0),unpackedVectorLength(0),member_vl_nwb_endp(0),member_vl_endp(0),totalNumBits(0) {}

  ~PackCliqueValue() {}

  // return the number of bits required for storing a vector of nodes
  // (or array with cardinalities).
  static unsigned numWordsRequiredFor(vector<RV*>& nodes,RV* extra=NULL);
  static unsigned numWordsRequiredFor(set<RV*>& nodes,RV* extra=NULL);
  static unsigned numWordsRequiredFor(const unsigned len, const unsigned *const cards);
  static unsigned numSpareBitsAvailableWith(vector<RV*>& nodes);


  // Return the number of words needed to hold an unpacked clique
  // value (i.e., the number of hidden variables in the clique).
  unsigned unPackedLen() { return unpackedVectorLength; }

  // Return the minimum number of unsigned words that are required to hold a
  // packed clique value.
  unsigned packedLen() { return numUnsignedInPackedVector; }
  // Return the minimum number of bits needed to hold a packed clique value.
  unsigned packedLenBits() { return totalNumBits; }
  // Return the minimum number of bytes needed to hold a packed clique value.
  unsigned packedLenBytes() { return (totalNumBits+7)/8; }
  // Return the number of splits (word boundary crossings) in this packing.
  unsigned numSplits() { return (member_vl_endp - member_vl_nwb_endp); }


  // Return the number of words needed to hold a packed clique value. I.e., this returns
  // the total amount of memory that is allocated to each packed entry, in units of worsd.
  unsigned packedLenInWords() { return packedLen(); }
  // Return the number of bytes needed to hold a packed clique value. I.e., this returns
  // the total amount of memory that is allocated to each packed entry, in units of bytes.
  unsigned packedLenInBytes() { return packedLen()*sizeof(unsigned); }
  // Return the number of bits needed to hold a packed clique value. I.e., this returns
  // the total amount of memory that is allocated to each packed entry, in units of bits. 
  unsigned packedLenInBits() { return packedLen()*sizeof(unsigned)*8; }




  // pack()
  // this routine assumes that both
  //         unpackedVectorLength > 0 
  // and that
  //      packedVectorLength > 0.
  inline void pack(const unsigned *const unpacked_vec,
		   unsigned *const packed_vec) {
    // zero out packed vector
    register unsigned *packed_vecp = packed_vec;
    register const unsigned *const packed_vec_endp = 
      packed_vec + numUnsignedInPackedVector;
    do {
      *packed_vecp++ = 0;
    } while (packed_vecp != packed_vec_endp);

    // do the packing, first the ones that do not
    // span a word boundaries
    register ValLocator* vl_p = valLocators.ptr;
    register const ValLocator *vl_nwb_endp = member_vl_nwb_endp;
    register const ValLocator *vl_endp = member_vl_endp;
    {
      // assume that at least one case did not span a word boundary
      do {
	const unsigned val = unpacked_vec[vl_p->loc];
	packed_vec[vl_p->start] |= (val << vl_p->startRightShift);
      } while (++vl_p != vl_nwb_endp);
    }
    // next the ones that span word boundaries
    {
      // assume that at least one case did not span a word boundary
      while (vl_p != vl_endp) {
	const unsigned val = unpacked_vec[vl_p->loc];
	packed_vec[vl_p->start] |= (val << vl_p->startRightShift);
	packed_vec[vl_p->start+1] |= 
	  ((val&(vl_p->nextMask<<vl_p->nextLeftShift)) >> vl_p->nextLeftShift);
	vl_p++;
      }
    }
  }

  // same as above, but that packs from an array of pointers to ints
  inline void pack(const unsigned *const *const unpacked_vec,
		   unsigned *const packed_vec) {
    // zero out packed vector
    register unsigned *packed_vecp = packed_vec;
    register const unsigned *const packed_vec_endp = 
      packed_vec + numUnsignedInPackedVector;
    do {
      *packed_vecp++ = 0;
    } while (packed_vecp != packed_vec_endp);

    // do the packing, first the ones that do not
    // span a word boundaries
    register ValLocator* vl_p = valLocators.ptr;
    register const ValLocator *vl_nwb_endp = member_vl_nwb_endp;
    register const ValLocator *vl_endp = member_vl_endp;
    {
      // assume that at least one case did not span a word boundary
      do {
	const unsigned val = (*unpacked_vec[vl_p->loc]);
	packed_vec[vl_p->start] |= (val << vl_p->startRightShift);
      } while (++vl_p != vl_nwb_endp);
    }
    // next the ones that span word boundaries
    {
      // assume that at least one case did not span a word boundary
      while (vl_p != vl_endp) {
	const unsigned val = (*unpacked_vec[vl_p->loc]);
	packed_vec[vl_p->start] |= (val << vl_p->startRightShift);
	packed_vec[vl_p->start+1] |= 
	  ((val&(vl_p->nextMask<<vl_p->nextLeftShift)) >> vl_p->nextLeftShift);
	vl_p++;
      }
    }
  }

  // version to do type change.
  // inline void pack(const int *const *const unpacked_vec,
  // int *const packed_vec) {
  // pack((const unsigned *const *const) unpacked_vec,
  //	 (unsigned*const)packed_vec);
  // }

  inline void unpack(const unsigned *const packed_vec,
		     unsigned *const unpacked_vec) {
    register ValLocator* vl_p = valLocators.ptr;
    register const ValLocator *vl_nwb_endp = member_vl_nwb_endp;
    register const ValLocator *vl_endp = member_vl_endp;
    // do the unpacking, first the ones
    // that do not span a word boundary
    {
      do {
	unpacked_vec[vl_p->loc] = 
	  (packed_vec[vl_p->start] & vl_p->startMask)
	    >> vl_p->startRightShift;
	vl_p++;
      } while (vl_p != vl_nwb_endp);
    }
    // next the ones that do span a word boundary
    {
      while (vl_p != vl_endp) {
	register unsigned res =
	  (packed_vec[vl_p->start] & vl_p->startMask)
	    >> vl_p->startRightShift;
	res |=
	  ((packed_vec[vl_p->start+1] & vl_p->nextMask) <<
	   vl_p->nextLeftShift);
	unpacked_vec[vl_p->loc] = res;	
	vl_p++;
      }
    }
  }

  // same as above, but version that unpacks to array of pointers to
  // ints.
  inline void unpack(const unsigned *const packed_vec,
		     unsigned **const unpacked_vec) {
    register ValLocator* vl_p = valLocators.ptr;
    register const ValLocator *vl_nwb_endp = member_vl_nwb_endp;
    register const ValLocator *vl_endp = member_vl_endp;
    // do the unpacking, first the ones
    // that do not span a word boundary
    {
      do {
	(*unpacked_vec[vl_p->loc]) = 
	  (packed_vec[vl_p->start] & vl_p->startMask)
	    >> vl_p->startRightShift;
	vl_p++;
      } while (vl_p != vl_nwb_endp);
    }
    // next the ones that do span a word boundary
    {
      while (vl_p != vl_endp) {
	register unsigned res =
	  (packed_vec[vl_p->start] & vl_p->startMask)
	    >> vl_p->startRightShift;
	res |=
	  ((packed_vec[vl_p->start+1] & vl_p->nextMask) <<
	   vl_p->nextLeftShift);
	(*unpacked_vec[vl_p->loc]) = res;	
	vl_p++;
      }
    }
  }

  // TODO: make a query() function that given a packed vector
  // and a position, returns the integer for that position.


  unsigned hamming_bit_distance(const unsigned *const vec1,
				const unsigned *const vec2);
  unsigned hamming_entry_distance(const unsigned *const vec1,
				  const unsigned *const vec2);
  unsigned hamming_weighted_entry_distance(const unsigned *const vec1,
					   const unsigned *const vec2);

  inline unsigned use_distance(const unsigned *const vec1,
			       const unsigned *const vec2)
  {
    return hamming_weighted_entry_distance(vec1,vec2);
    // return hamming_bit_distance(vec1,vec2);
  }

  // compare two packed values, -1 if vec1<vec2, 0 if vec1=vec2, +1 if vec1>vec2
  int compare(const unsigned * vec1,
	      const unsigned * vec2) {
    const unsigned *const vec1_endp = vec1 + packedLenInWords();
    do {
      if (*vec1 < *vec2)
	return -1;
      else if (*vec1 > *vec2)
	return +1;
      vec1++; 
      vec2++;
    } while (vec1 != vec1_endp);
    return 0;
  }

};


#endif

