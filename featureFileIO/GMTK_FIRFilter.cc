
/*
 * GMTK_FIRFilter.cc
 * 
 * Written by Richard Rogers <rprogers@ee.washington.edu>
 *
 * Copyright (c) 2012, < fill in later >
 * 
 * < License reference >
 * < Disclaimer >
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#if HAVE_HG_H
#include "hgstamp.h"
#endif

#include "error.h"
#include "general.h"
VCID(HGID)

#include <string.h>
#include <stdio.h>

#include "GMTK_FIRFilter.h"

FIRFilter::FIRFilter(char const *fileName, Filter *nextFilter) 
  : Filter(nextFilter)
{
  FILE *f = fopen(fileName, "r");
  if (!f) {
    error("FIRFilter: cannot open '%s' for reading\n", fileName);
  }
  if (fscanf(f, "%u %u", &order, &numFeatures) != 2) {
    error("FIRFilter: error reading FIR filter size from '%s'\n", fileName);
  }

  c = new float[numFeatures];
  if (!c) {
    error("FIRFilter: failed to allocate constant vector\n");
  }
  unsigned Bsize = (order+1) * numFeatures;
  B = new float[Bsize];
  if (!B) {
    error("FIRFilter: failed to allocate coefficient matrix\n");
  }
  for (unsigned i=0; i < Bsize; i+=1) {
    if (fscanf(f,"%e", B+i) != 1) {
      error("FIRFilter: error reading coefficient matrix\n");
    }
  }
  for (unsigned i=0; i < numFeatures; i+=1) {
    if (fscanf(f,"%e", c+i) != 1) {
      error("FIRFilter: error reading constant vector\n");
    }
  }
  fclose(f);
  buffer=NULL; buffSize=0;
}


subMatrixDescriptor *
FIRFilter::getRequiredInput(unsigned first, unsigned count, 
			    unsigned inputContinuous, unsigned inputDiscrete,
			    unsigned inputTotalFrames)
{
  subMatrixDescriptor *nextFilterInput = NULL;
  if (nextFilter) {
    unsigned outputContinuous = inputContinuous;
    unsigned outputDiscrete   = inputDiscrete;
    unsigned outputFrames     = inputTotalFrames;
    nextFilterInput = nextFilter->getRequiredInput(first, count, 
						   outputContinuous, outputDiscrete,
						   outputFrames);
    assert(nextFilterInput);
    first = nextFilterInput->firstFrame;
    count = nextFilterInput->numFrames;
  } 
  unsigned history;
  if (first >= order) {
    first -= order;
    count += order;
    history = order;
  } else {
    count += first;
    history = first;
    first = 0;
  }
  return new subMatrixDescriptor(first, count, history, 0, inputContinuous, 
				 inputDiscrete, inputTotalFrames, nextFilterInput);
}


subMatrixDescriptor
FIRFilter::describeLocalOutput(subMatrixDescriptor const &inputDescription) {
  subMatrixDescriptor myOutput = inputDescription;
  myOutput.firstFrame += inputDescription.historyFrames;
  myOutput.numFrames  -= inputDescription.historyFrames;
  myOutput.historyFrames = 0;
  myOutput.futureFrames = 0;
  myOutput.next = NULL;
  return myOutput;
}


Data32 const *
FIRFilter::localTransform(Data32 const *inputSubMatrix, 
			  subMatrixDescriptor const &inputDescription,
			  subMatrixDescriptor *outputDescription) 
{

  // FIXME - error checking that B and c are compatible with X

  unsigned stride = inputDescription.numContinuous + inputDescription.numDiscrete;

  subMatrixDescriptor myOutput = describeLocalOutput(inputDescription);
  unsigned needed = stride * myOutput.numFrames;

  //  printf("xfrm in  [%u, %u, %u, %u]  :  stride %u  :  out [%u, %u]\n", inputDescription.firstFrame, inputDescription.historyFrames, inputDescription.numFrames, inputDescription.futureFrames, stride, myOutput.firstFrame, myOutput.numFrames);
  if (needed > buffSize) {
    buffer = (Data32 *) realloc(buffer, needed * sizeof(Data32));
    assert(buffer);
    buffSize = needed;
  }

  for (unsigned i=0; i < myOutput.numFrames; i+=1) {
    float *outputCont = (float *)(buffer + i * stride);
    if (c) {
      memcpy(outputCont, c, myOutput.numContinuous * sizeof(Data32));
    } else {
      memset(outputCont, 0, myOutput.numContinuous * sizeof(Data32));
    }
    Uint32 *outputDisc = (Uint32 *)(buffer + i * stride + myOutput.numContinuous);
    Uint32 *inputDisc  = (Uint32 *)(inputSubMatrix + 
				    (i + inputDescription.historyFrames) * stride + 
				    inputDescription.numDiscrete);
    memcpy(outputDisc, inputDisc, myOutput.numDiscrete * sizeof(Data32));
  }

  unsigned availableHistory = inputDescription.historyFrames;
  for (unsigned outr=0; outr < myOutput.numFrames; outr+=1) {
    float *outputRow = (float *)(buffer + outr * stride);
    for (unsigned h=0; h <= availableHistory; h+=1) {
      float *inputRow  = (float *)(inputSubMatrix + (outr + inputDescription.historyFrames - h) * stride);
      for (unsigned j=0; j < myOutput.numContinuous; j+=1) {
	outputRow[j] += inputRow[j] * (B + h * myOutput.numContinuous)[j];
      }
    }
    availableHistory = (availableHistory < order) ? availableHistory + 1  :  order;
  }

  if (outputDescription) *outputDescription = myOutput;
  return buffer;
}