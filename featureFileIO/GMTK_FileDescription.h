
/* GMTK_FileDescription: stores info related to input stream 
 * 
 * Written by Katrin Kirchhoff <katrin@ee.washington.edu>
 *
 * Copyright (C) 2001 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 * See COPYING or http://opensource.org/licenses/OSL-3.0
 *
 */

#ifndef GMTK_FileDescription_h
#define GMTK_FileDescription_h

#include <ctype.h>
#include "bp_range.h"
#include "pfile.h"


enum {
  RAWBIN, 
  RAWASC, 
  FLATASC,
  PFILE,
  HTK,
  HDF5,
  GMTK
};

/* file description class: contains information about (list of) input files */


class FileDescription {

public:

  unsigned nFloats;        // number of floats (cont. features) in file
  unsigned nInts;          // number of ints (disc. features) in file
  unsigned dataFormat;     // file format

  unsigned nFloatsUsed;    // number of floats actually used (of input)
  unsigned nIntsUsed;      // number of ints actually used (of input)

  char *fofName;           // this file's file name (name of list of file names)
  FILE *fofFile;           // this file (list of file names)
  size_t fofSize;          // size of list of file names

  bool bswap;              // true if file needs to be byte-swapped

  InFtrLabStream_PFile *pfile_istr;  // pfile input stream

  char **dataNames;        // pointers to individual filenames (into fofBuf)
  FILE *curDataFile;       // current data file
  size_t curDataSize;      // size of current data file
 
  size_t curPos;           // index of current position in list of files

  BP_Range *cont_rng;      // range of cont. features used
  BP_Range *disc_rng;      // range of disc. features used

  FileDescription(const char *,
		  const char *, 
		  const char *,
                  unsigned *, 
		  unsigned *, 
		  unsigned *,
		  bool *,
		  unsigned);

  ~FileDescription();

  size_t readFof(FILE *);       // read file of file names
};

#endif
