
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>


/*
 * Written by Richard Rogers <rprogers@uw.edu>
 *
 * Copyright (C) 2012 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 * See COPYING or http://opensource.org/licenses/OSL-3.0
 *
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
static const char * gmtk_version_id = PACKAGE_STRING;
#  ifdef HAVE_HG_H
#    include "hgstamp.h"
#  endif

#else 
// TODO: automate the process of updating this string.
static const char * gmtk_version_id = "GMTK Version 0.2b Tue Jan 20 22:59:41 2004";
#endif

#include "general.h"
#include "error.h"
#include "debug.h"
#include "arguments.h"

#include "GMTK_ObservationSource.h"
#include "GMTK_FileSource.h"
#include "GMTK_CreateFileSource.h"
#if 0
#include "GMTK_ASCIIFile.h"
#include "GMTK_FlatASCIIFile.h"
#include "GMTK_PFileFile.h"
#include "GMTK_HTKFile.h"
#include "GMTK_HDF5File.h"
#include "GMTK_BinaryFile.h"
#endif
#include "GMTK_Stream.h"
unsigned chunkSize = 1;


#define GMTK_ARG_OBS_FILES
#define GMTK_ARG_CPP_CMD_OPTS
#define GMTK_ARG_OBS_MATRIX_XFORMATION
#define GMTK_ARG_FILE_RANGE_OPTIONS
#define GMTK_ARG_START_END_SKIP
#define GMTK_ARG_HELP
#define GMTK_ARG_VERSION

#define GMTK_ARGUMENTS_DEFINITION
#include "ObsArguments.h"
#undef GMTK_ARGUMENTS_DEFINITION

Arg Arg::Args[] = {
#define GMTK_ARGUMENTS_DOCUMENTATION
#include "ObsArguments.h"
#undef GMTK_ARGUMENTS_DOCUMENTATION
  // final one to signal the end of the list
  Arg("chunkSize", Arg::Opt,chunkSize, "# of frames to load/print at once"),
  Arg()
};


int 
main(int argc, char *argv[]) {

  bool parse_was_ok = Arg::parse(argc,(char**)argv);

  if(help) {
    Arg::usage();
    exit(0);
  }
  if(!parse_was_ok) {
    Arg::usage(); exit(-1);
  }
  if (print_version_and_exit) {
#ifdef HAVE_CONFIG_H
    printf("%s (Mercurial id: %s)\n",gmtk_version_id,HGID);
#else
    printf("%s\n", gmtk_version_id);
#endif
    exit(0);
  }
  infoMsg(IM::Max,"Finished parsing arguments\n");
#define GMTK_ARGUMENTS_CHECK_ARGS
#include "ObsArguments.h"
#undef GMTK_ARGUMENTS_CHECK_ARGS

  infoMsg(IM::Max,"Opening Files ...\n");

  FileSource *f = instantiateFileSource();
  for (unsigned j=0; j < f->numSegments(); j+=1) {
    assert(f->openSegment(j));
    printf("Processing sentence %u\n", j);
    unsigned k;
    for (k=0; k < f->numFrames() % chunkSize; k+=1) {
      //      printf("%03u %03u", j, k);
      printf("%u %u", j, k);
      Data32 const *buf = f->loadFrames(k,1);
      for (unsigned ff=0; ff < f->numContinuous(); ff+=1)
	printf(" %f", *((float *)(buf++)));
      for (unsigned ff=0; ff < f->numDiscrete(); ff+=1)
	printf(" %d", *((int *)(buf++)));
      printf("\n");
    }
    for ( ; k < f->numFrames(); k+=chunkSize) {
      Data32 const *buf = f->loadFrames(k,chunkSize);
      for (unsigned kk=k; kk < k+chunkSize; kk+=1) {
	//	printf("%03u %03u", j, kk);
	printf("%u %u", j, kk);
	for (unsigned ff=0; ff < f->numContinuous(); ff+=1)
	  printf(" %f", *((float *)(buf++)));
	for (unsigned ff=0; ff < f->numDiscrete(); ff+=1)
	  printf(" %d", *((int *)(buf++)));
	printf("\n");
      }
    }
  }
  
  exit(0);
}
