
/*
 * GMTK_FileSource.cc
 * 
 * Written by Richard Rogers <rprogers@ee.washington.edu>
 *
 * Copyright (c) 2011, < fill in later >
 * 
 * < License reference >
 * < Disclaimer >
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "error.h"
#include "general.h"
#include "machine-dependent.h"
#include "debug.h"

#include "GMTK_FilterFile.h"
#include "GMTK_FileSource.h"
#include "GMTK_Filter.h"

#define BUFFER_SIZE (1024*1024)
  // each file provides its data for the requested submatrix,
  // transformed and "ready to eat." So the FileSource
  // just manages assembling them to satisfy the loadFrames()
  // calls, prefetching/caching. For archipelagos, each thread
  // gets its own FileSource (all aimed at the same files, of course).
FileSource::FileSource(unsigned _nFiles, ObservationFile *file[], 
		       unsigned bufferSize,
		       char const *_globalFrameRangeStr, 
		       unsigned const *sdiffact, unsigned const *fdiffact,
		       unsigned startSkip, unsigned endSkip,
		       Filter *posttrans, int justificationMode, int ftrcombo) 
{
  initialize(_nFiles, file, bufferSize, sdiffact, fdiffact, 
	     _globalFrameRangeStr, startSkip, endSkip, posttrans, justificationMode, ftrcombo);
}


#if 0
static
void
dumpFrames(Data32 *buf, unsigned nFloat, unsigned nInt,
	   unsigned nFrames)
{
  for (unsigned j=0; j < nFrames; j+=1) {
    float *fp = (float *)buf;
    printf("%3u: ", j);
    for (unsigned i=0; i < nFloat; i+=1) {
      printf(" %f", *(fp++));
    }
    unsigned *ip = (unsigned *)fp;
    for (unsigned i=0; i < nInt; i+=1) {
      printf(" %u", *(ip++));
    }
    printf("\n");
  }
}
#endif
    
static
unsigned
checkNumSegments(unsigned nFiles, ObservationFile *file[],
		 unsigned const *sdiffact)
{
  unsigned min_len=file[0]->numLogicalSegments();
  unsigned max_len=min_len;
  if(max_len == 0) 
    warning("WARNING: FileSource::initialize:checkNumSegments:  file 0 is empty\n");

  bool got_error    = false;
  bool got_truncate = false;
  bool got_expand   = false;

  for(unsigned file_no=0; file_no < nFiles; ++file_no) {
    unsigned len=file[file_no]->numLogicalSegments();
    if(len < min_len) min_len=len;
    if(len > max_len) max_len=len;

    if(	(sdiffact != NULL && sdiffact[file_no] == SEGMATCH_ERROR) ) {
      got_error = true;
    // the default is always to truncate (i.e. NULL case below)
    } else if(sdiffact == NULL || sdiffact[file_no] == SEGMATCH_TRUNCATE_FROM_END) {
      got_truncate =true;
    } else if( sdiffact != NULL &&
	     (sdiffact[file_no] == SEGMATCH_REPEAT_LAST ||
	      sdiffact[file_no] == SEGMATCH_WRAP_AROUND)
	     )
    {
      got_expand = true;
      fprintf(stderr,"got expand %u for %u\n", sdiffact[file_no], file_no);
    }
  }
  if(max_len == 0)
    error("ERROR: FileSource::intialize:checkNumSegments:  all streams have zero length");

  // error checking
  if(min_len != max_len) {
    if(got_error)
      error("ERROR: FileSource:initialize:checkNumSegments: Streams have different # of segments (min=%d, max=%d)",min_len,max_len);

    if(got_truncate && got_expand)
      error("ERROR: FileSource::initialize:checkNumSegments: Cannot specify both truncate and expand actions when using observation files of different lengths");
  }

  if(got_truncate) {
    // Adjust length of streams in the truncate case
    // We don't need to do that in the expand case
    if(min_len == 0)
      error("ERROR: FileSource::initialize:checkNumSegments: minimum stream length is zero");
    return min_len;
  } else {
    return max_len;  // if there is no expand, it means min_len=max_len
  }
}


static
unsigned
checkNumFrames(unsigned nFiles, ObservationFile *file[],
	       unsigned const *fdiffact)
{
  unsigned min_len=file[0]->numLogicalFrames();
  unsigned max_len=min_len;
  if(max_len == 0) 
    warning("WARNING: FileSource::openSegment:checkNumFrames:  segment 0 is empty\n");

  bool got_error    = false;
  bool got_truncate = false;
  bool got_expand   = false;

  for(unsigned file_no=0; file_no < nFiles; file_no += 1) {
    unsigned len=file[file_no]->numLogicalFrames();
    if(len < min_len) min_len=len;
    if(len > max_len) max_len=len;

    if (fdiffact != NULL) {
      switch (fdiffact[file_no]) {
      case FRAMEMATCH_TRUNCATE_FROM_START:
      case FRAMEMATCH_TRUNCATE_FROM_END:
	got_truncate = true;
//printf("file %u with %u frames to truncate -> %u\n", file_no, len, min_len);
	break;
      case FRAMEMATCH_REPEAT_FIRST:
      case FRAMEMATCH_REPEAT_LAST:
      case FRAMEMATCH_EXPAND_SEGMENTALLY:
	got_expand = true;
//printf("file %u with %u frames to expand -> %u\n", file_no, len, max_len);
	break;
      case FRAMEMATCH_ERROR:
	got_error = true;
//printf("file %u with %u frames to error if target length != %u\n", file_no, len, len);
	break;
      }
    } else {
      got_error = true;
//printf("file %u with %u frames to error if target length != %u\n", file_no, len, len);
    }
    if(got_truncate && got_expand)
      // FIXME - match ObservationMatrix error message?
      error("ERROR: FileSource::openSegment:checkNumFrames: Cannot specify both truncate and expand actions when segments have different lengths");
    
  }
  if(max_len == 0)
    error("ERROR: FileSource::openSegment:checkNumFrames:  all segments have zero length");
  
#define EXPANSIVE(action,file_no) ( action && (action[file_no] == FRAMEMATCH_REPEAT_FIRST || \
                                               action[file_no] == FRAMEMATCH_REPEAT_LAST  || \
                                               action[file_no] == FRAMEMATCH_EXPAND_SEGMENTALLY) )

#define CONTRACTIVE(action,file_no) ( action && (action[file_no] == FRAMEMATCH_TRUNCATE_FROM_START || \
                                                 action[file_no] == FRAMEMATCH_TRUNCATE_FROM_END) )

  for(unsigned file_no=0; file_no < nFiles; ++file_no) {
    unsigned len=file[file_no]->numLogicalFrames();
    if( got_expand && len < max_len && !EXPANSIVE(fdiffact,file_no) ) {
      // FIXME - filenames... match error messages
      error("ERROR: observation file %u needs an -fdiffact%u that expands\n", file_no+1, file_no+1);
    }
    if ( got_truncate && len > min_len && !CONTRACTIVE(fdiffact,file_no) ) {
      error("ERROR: observation file %u needs an -fdiffact%u that truncates\n", file_no+1, file_no+1);
    }
    if (!fdiffact || fdiffact[file_no] == FRAMEMATCH_ERROR) {
      if (got_truncate && len != min_len) {
	error("ERROR: observation file %u needs an -fdiffact%u that truncates\n", file_no+1, file_no+1);
      } else if (len != max_len) {
	error("ERROR: observation file %u needs an -fdiffact%u that expands\n", file_no+1, file_no+1);
      }
    }
  }
  
  if(got_truncate) {
    // Adjust length of segments in the truncate case
    // We don't need to do that in the expand case
    if(min_len == 0)
      error("ERROR: FileSource:openSegment:checkNumFrames: minimum segment length is zero");

//printf("truncating to %u\n", min_len);
    return min_len;
  } else {
//printf("expanding to %u\n", max_len);
    return max_len;  // if there is no expand, it means min_len == max_len
  }
}


void 
FileSource::initialize(unsigned nFiles, ObservationFile *file[], 
		       unsigned bufferSize,
		       unsigned const *sdiffact, unsigned const *fdiffact,
		       char const *globalFrameRangeStr, 
		       unsigned startSkip, unsigned endSkip,
		       Filter *posttrans, int justificationMode, int ftrcombo) 
{
  assert(file);
  assert(nFiles);
  assert( 0 <= justificationMode && justificationMode <= FRAMEJUSTIFICATION_RIGHT );
  this->nFiles = nFiles;
  this->globalFrameRangeStr = globalFrameRangeStr;
  this->globalFrameRange = NULL;
  this->_startSkip = startSkip;
  this->_endSkip = endSkip;
  this->ftrcombo = ftrcombo;
  this->sdiffact = sdiffact;
  this->fdiffact = fdiffact;
  this->filter = posttrans;
  this->justificationMode = justificationMode;
  justificationOffset = 0;
  _minPastFrames = 0;
  _minFutureFrames = 0;
  this->file = new ObservationFile *[nFiles];
  for (unsigned i=0; i < nFiles; i+=1) {
    assert(file[i]);
    this->file[i] = file[i];
  }
  cookedBuffer = new Data32[bufferSize];
  if (!cookedBuffer) {
    error("ERROR: FileSource::intialize: failed to allocate frame buffer");
  }
  this->bufferSize = bufferSize;
  numBufferedFrames = 0;
  
  floatStart   = new Data32 *[nFiles];
  intStart     = new Data32 *[nFiles];
  this->segment = -1;         // no openSegment() call yet
  this->_numContinuous = -1;  // compute these when needed
  this->_numDiscrete = -1;
  // FIXME - FileSource should know about feature subranges unless the file format can do it better
  //  no it shouldn't - ObservationFile base class does naive feature subranges, subclasses
  //  can over-ride the *Logical* methods if they can do better
  unsigned offset = 0;
  unsigned maxFloats = 0;
  for (unsigned i=0; i < nFiles; i+=1) {
    floatStart[i] = cookedBuffer + offset;
    unsigned nlc = file[i]->numLogicalContinuous();
    if (ftrcombo == FTROP_NONE) offset += nlc;
    if (maxFloats < nlc) maxFloats = nlc;
  }
  if (ftrcombo != FTROP_NONE) {
    offset = maxFloats;
  }

  for (unsigned i=0; i < nFiles; i+=1) {
    intStart[i] = cookedBuffer + offset;
    offset += file[i]->numLogicalDiscrete();
  }
  bufStride = offset;

  _numSegments = checkNumSegments(nFiles, file, sdiffact);

  bufferFrames = bufferSize / bufStride;
}


// adjust seg according to -sdiffactX
unsigned 
FileSource::adjustForSdiffact(unsigned fileNum, unsigned seg) {
  if (!sdiffact || sdiffact[fileNum] == SEGMATCH_TRUNCATE_FROM_END)
    return seg;
  if (sdiffact[fileNum] == SEGMATCH_REPEAT_LAST) {
    if (seg < file[fileNum]->numLogicalSegments())
      return seg;
    else
      return file[fileNum]->numLogicalSegments() - 1;
  } else if (sdiffact[fileNum] == SEGMATCH_WRAP_AROUND) {
    return seg % file[fileNum]->numLogicalSegments();
  } else if (sdiffact[fileNum] == SEGMATCH_ERROR) {
    return seg;
  } else 
    error("ERROR: FileSource::adjustForSdiffact: unknown -sdiffact%u %d\n",
	  fileNum, sdiffact[fileNum]);
  return 0; // impossilbe to get here, but compiler warns about no return
}


  // Begin sourcing data from the requested segment.
  // Must be called before any other operations are performed on a segment.
bool 
FileSource::openSegment(unsigned seg) {
  // FIXME - FileSource should know about segment subranges unless the file format can do it better
  //  no it shouldn't - ObservationFile base class does naive feature subranges, subclasses
  //  can over-ride the *Logical* methods if they can do better

  if (seg >= _numSegments) {
    error("ERROR: FileSource::openSegment: requested segment %u, but only up to %u are available\n", seg, _numSegments-1);
  }
  bool success = true;

  // FIXME - error handling
  for (unsigned i=0; i < nFiles; i+=1) {
    unsigned s = adjustForSdiffact(i, seg);
    success = success && file[i]->openLogicalSegment(s);
  }
  this->segment = seg;

  unsigned numInputFrames = checkNumFrames(nFiles, file, fdiffact);
#ifndef JEFFS_STRICT_DEBUG_OUTPUT_TEST
  infoMsg(IM::ObsFile,IM::Low,"%u input frames in segment %d\n", numInputFrames, seg);
#endif
  if (globalFrameRange) delete globalFrameRange;
  // FIXME - max should be fdiffact-aware
  // note that -posttrans is not allowed to alter the # of frames
  globalFrameRange = new Range(globalFrameRangeStr, 0, numInputFrames);
  _numCacheableFrames = globalFrameRange->length();
  _numFrames = _numCacheableFrames;

  // FIXME - double check this
  if (_numCacheableFrames < _startSkip + _endSkip) {
    error("ERROR: FileSource::openSegment: segment has only %d frames, but there must be at least %u to support start/end skip",
	  _numCacheableFrames, _startSkip + _endSkip);
  }
  _numFrames -= _startSkip; // reserve frames at start of segment
  _numFrames -= _endSkip;   // reserve frames at end of segment
  
  justificationOffset = 0;
  numBufferedFrames = 0;
  return success;
}


void 
FileSource::justifySegment(unsigned numUsableFrames) {
  if (numUsableFrames > _numFrames) {
    error("ERROR: FileSource::justifySegment: numUsableFrames (%u) must not be larger than the number of available frames (%u)",
	  numUsableFrames, _numFrames);
  }
  assert(segment >= 0);
  switch (justificationMode) {
  case FRAMEJUSTIFICATION_LEFT:
    justificationOffset = 0;
    break;
  case FRAMEJUSTIFICATION_CENTER:
    justificationOffset = (_numFrames - numUsableFrames) / 2;
    break;
  case FRAMEJUSTIFICATION_RIGHT:
    justificationOffset = _numFrames - numUsableFrames;
    break;
  default:
    error("ERROR: FileSource::justifySegment: unknown justification mode %d", justificationMode);
  }
  infoMsg(IM::ObsFile,IM::Low,"justification mode %u offset = %u\n", justificationMode, justificationOffset);
  _numFrames = numUsableFrames;
}


// frameNum      pre-expansion frame # we need repetition count for
// deltaT        difference in frames between target length and actual segment length
// firstFrame    first frame # desired (post-expansion)
// frameCount    # of desired frames (post-expansion)
// segLength     total # of frames in pre-expansion segment

static unsigned 
repCount(unsigned frameNum, unsigned const *fdiffact, unsigned fileNum, 
	 unsigned deltaT, unsigned firstFrame, unsigned frameCount, unsigned segLength)
{
  if (!fdiffact) return 1;

  if (fdiffact[fileNum] == FRAMEMATCH_REPEAT_FIRST && frameNum == 0) {
    return frameCount > deltaT + 1 - firstFrame  ?  deltaT + 1 - firstFrame  :  frameCount;
  } else if (fdiffact[fileNum] == FRAMEMATCH_REPEAT_LAST && frameNum == segLength-1) {
    return firstFrame >= segLength  ?
      frameCount  :  frameCount - (segLength - firstFrame) + 1;
  } else if (fdiffact[fileNum] == FRAMEMATCH_EXPAND_SEGMENTALLY) {

    unsigned frameReps = (segLength + deltaT) / segLength;
    unsigned remainder = (segLength + deltaT) % segLength;

    unsigned preExpFirst = (firstFrame < remainder * (frameReps+1)) ? 
      (firstFrame / (frameReps+1)) : 
      (remainder + (firstFrame - remainder * (frameReps + 1)) / frameReps);

    unsigned postExpLast = firstFrame + frameCount - 1;
    unsigned preExpLast = (postExpLast < remainder * (frameReps+1)) ? 
      (postExpLast / (frameReps+1)) : 
      (remainder + (postExpLast - remainder * (frameReps + 1)) / frameReps);

    assert(preExpFirst <= frameNum && frameNum <= preExpLast);

    if (preExpFirst == preExpLast) return frameCount;

    unsigned runLength = (frameNum < remainder) ?  frameReps + 1  :  frameReps;

    if (frameNum == preExpFirst) {
      if (firstFrame < remainder * (frameReps + 1)) 
	firstFrame -=  remainder * (frameReps + 1);
      unsigned positionInRun = firstFrame % runLength;
      return runLength - positionInRun;
    }

    if (frameNum == preExpLast) {
      if (postExpLast < remainder * (frameReps + 1)) 
	postExpLast -=  remainder * (frameReps + 1);
      unsigned positionInRun = postExpLast % runLength;
      return 1 + positionInRun;
    }

    return runLength;
  }
  return 1;
}


Data32 const*
FileSource::loadFrames(unsigned bufferIndex, unsigned first, unsigned count) {
  // FIXME - startSkip and endSkip

  // apply -gpr
  unsigned oldFirst = first;
  first = globalFrameRange->index(first);
  count = globalFrameRange->index(oldFirst + count - 1) - first + 1;

  assert(0 <= bufferIndex && bufferIndex < bufferFrames);
  assert(0 < count && count < bufferFrames-bufferIndex);
  
  unsigned buffOffset = bufferIndex * bufStride;

  memset(cookedBuffer+buffOffset, 0, sizeof(Data32) * numFeatures() * count);

  if (first > _numCacheableFrames || first + count > _numCacheableFrames) {
    error("ERROR: FileSource::loadFrames: requested frames [%u,%u), but %u is the last available frame\n", first, first+count, _numCacheableFrames);
  }
  subMatrixDescriptor *inputDesc = NULL;
  if (filter) {
    // find out what we need to feed the filter to produce
    // the requested frames
    inputDesc =
      filter->getRequiredInput(first, count, numContinuous(),
			       numDiscrete(), numFrames());
    
    // FIXME:  rawBuffer -> filter -> cookedBuffer
    // assemble the -posttrans filter input
    first = inputDesc->firstFrame;
    count = inputDesc->numFrames;
  }

  for (unsigned int i=0; i < nFiles; i+=1) {
    
    // in case we need to adjust for fdiffact
    unsigned adjFirst, adjCount, deltaT; 
    if (fdiffact && fdiffact[i] == FRAMEMATCH_REPEAT_FIRST) {
      deltaT = _numCacheableFrames - file[i]->numLogicalFrames();
      assert(_numCacheableFrames >= file[i]->numLogicalFrames());
      adjFirst =  first >= deltaT  ?  first - deltaT  :  0;
      adjCount = count;
    } else if (fdiffact && fdiffact[i] == FRAMEMATCH_REPEAT_LAST) {
      deltaT = _numCacheableFrames - file[i]->numLogicalFrames();
      assert(_numCacheableFrames >= file[i]->numLogicalFrames());
      adjFirst = first < file[i]->numLogicalFrames() ?
        first  :  file[i]->numLogicalFrames() - 1;
      adjCount =  first + count <= file[i]->numLogicalFrames()  ?
	count  :  file[i]->numLogicalFrames() - adjFirst;
    } else if (fdiffact && fdiffact[i] == FRAMEMATCH_EXPAND_SEGMENTALLY) {
      deltaT = _numCacheableFrames - file[i]->numLogicalFrames();
      assert(_numCacheableFrames >= file[i]->numLogicalFrames());

      unsigned frameReps = _numCacheableFrames / file[i]->numLogicalFrames();
      unsigned remainder =  _numCacheableFrames % file[i]->numLogicalFrames();
      unsigned last = first + count - 1;
      unsigned adjLast;
      if (first < remainder * (frameReps+1)) {
	adjFirst = first / (frameReps + 1);
      } else {
	adjFirst = remainder + 
	  (first - remainder * (frameReps + 1)) / frameReps;
      }
      if (last < remainder * (frameReps+1)) {
	adjLast = last / (frameReps + 1);
      } else {
	adjLast = remainder + 
	  (last - remainder * (frameReps + 1)) / frameReps;
      }
      assert(adjLast >= adjFirst);
      adjCount = adjLast - adjFirst + 1;
      assert(adjCount >= 1);

    } else if (fdiffact && fdiffact[i] == FRAMEMATCH_TRUNCATE_FROM_END) {
      // don't have to do anything for this case - the error checking
      // prevents accessing any frames after numFrames()
      deltaT = file[i]->numLogicalFrames() - _numCacheableFrames;
      assert(file[i]->numLogicalFrames() >= _numCacheableFrames);
      adjFirst = first;
      adjCount = count;
    } else if (fdiffact && fdiffact[i] == FRAMEMATCH_TRUNCATE_FROM_START) {
      deltaT = file[i]->numLogicalFrames() - _numCacheableFrames;
      assert(file[i]->numLogicalFrames() >= _numCacheableFrames);
      adjFirst = first + deltaT;
      adjCount = count;
    } else {
      deltaT   = 0;
      adjFirst = first;
      adjCount = count;
    }
    // if -fdiffacti expands this segment, we may need to
    // fiddle with (first,count):
    //   rf  numFrames() - file[i]->numLogicalFrames() extra first frames
    //   rl  numFrames() - file[i]->numLogicalFrames() extra last  frames
    //   se  approx. numFrames() / file[i]->numLogicalFrames extra of each frame
    Data32 const *fileBuf = file[i]->getLogicalFrames(adjFirst,adjCount);
    assert(fileBuf);
    Data32 *dst = floatStart[i] + buffOffset;
    Data32 const *src = fileBuf;
    unsigned srcStride = file[i]->numLogicalFeatures();
    for (unsigned f=0; f < adjCount; f += 1) {
      unsigned frameReps = repCount(adjFirst + f,fdiffact, i, deltaT, first, count, file[i]->numLogicalFrames());
      assert(0 < frameReps && frameReps <= count);
      for (unsigned j=0; j < frameReps; j+=1) {
	float *fdst = (float *) dst;
	float *fsrc = (float *) src;
	switch(ftrcombo) {
	case FTROP_NONE:
	  memcpy((void *)dst, (const void *)src, file[i]->numLogicalContinuous() * sizeof(Data32));
//        dst += bufStride;
	  break;
	case FTROP_ADD:
	  for (unsigned k=0; k < file[i]->numLogicalContinuous(); k+=1) {
	    fdst[k] += fsrc[k];
	  }
	  break;
	case FTROP_SUB:
	  if (i == 0) {
	    memcpy((void *)dst, (const void *)src, file[i]->numLogicalContinuous() * sizeof(Data32));
	  } else {
	    for (unsigned k=0; k < file[i]->numLogicalContinuous(); k+=1) {
	      fdst[k] -= fsrc[k];
	    }
	  }
	  break;
	case FTROP_MUL:
	  if (i == 0) {
	    memcpy((void *)dst, (const void *)src, file[i]->numLogicalContinuous() * sizeof(Data32));
	  } else {
	    for (unsigned k=0; k < file[i]->numLogicalContinuous(); k+=1) {
	      fdst[k] *= fsrc[k];
	    }
	  }
	  break;
	case FTROP_DIV:
	  if (i == 0) {
	    memcpy((void *)dst, (const void *)src, file[i]->numLogicalContinuous() * sizeof(Data32));
	  } else {
	    for (unsigned k=0; k < file[i]->numLogicalContinuous(); k+=1) {
	      if (fsrc[k] != 0) {
		fdst[k] /= fsrc[k];
	      }
	    }
	  }
	  break;
	default:
	  error("ERROR: unknown -comb option value %d", ftrcombo);
	}
	dst += bufStride;
      }
      src += srcStride;
    }

    src = fileBuf + file[i]->numLogicalContinuous();
    dst = intStart[i] + buffOffset;
    for (unsigned f=0; f < adjCount; f += 1) {
      unsigned frameReps = repCount(adjFirst + f,fdiffact, i, deltaT, first, count, file[i]->numLogicalFrames());
      assert(0 < frameReps && frameReps <= count);
      for (unsigned j=0; j < frameReps; j+=1) {
        memcpy((void *)dst, (const void *)src, file[i]->numLogicalDiscrete() * sizeof(Data32));
	dst += bufStride;
      }
      src += srcStride;
    }
  }
  // if requested frames are already in cookedBuffer
  //   return &cookedBuffer[index of first requested frame]
  // adjust (first,count) for prefetching -> (first',count')
  // adjust cookedBuffer destination
  // getFrames(first', count') from each file into cookedBuffer
  // return &cookedBuffer[destination of first]
#if 0
for (unsigned frm=0; frm < count; frm+=1) {
  printf("   F %u @ %u :", first+frm, bufferIndex+frm);
  float *ppp = (float *)(cookedBuffer+buffOffset);
  for (unsigned j=0; j < 2; j+=1) {
    printf(" %f", *(ppp++));
  }
  printf("\n");
}
#endif
  if (filter) {
    Data32 const *result = filter->transform(cookedBuffer+buffOffset, *inputDesc);
    subMatrixDescriptor::freeSMD(inputDesc);
    if (result != cookedBuffer + buffOffset) {
      memcpy(cookedBuffer + buffOffset, result, count * bufStride * sizeof(Data32));
    }
  }
#if 0
  warning("fetching [%u,%u) -> %u", first, first+count, buffOffset);
for (unsigned x=0; x < count; x+=1) {
  printf("%3u:", x+first);
  for (unsigned c=0; c < numContinuous(); c+=1) printf(" %f", *((float    *)(cookedBuffer+buffOffset+c+x*bufStride)));
  for (unsigned d=0; d < numDiscrete()  ; d+=1) printf(" %u", *((unsigned *)(cookedBuffer+buffOffset+d+x*bufStride+numContinuous())));
  printf("  @ %u\n",buffOffset+x*bufStride );
}
#endif
  return cookedBuffer + buffOffset;
}


#define window 20
#define delta 5

// FIXME - have to ensure startSkip frames before and endSkip frames after
// every request ?
Data32 const *
FileSource::loadFrames(unsigned first, unsigned count) {
  unsigned preFirst;  // first frame # to prefetch
  unsigned preCount;  // # of frames in prefetch request

#if 0
unsigned requestedFirst = first; 
unsigned requestedCount = count;
#endif

  // FIXME - adjust first+count checking for endSkip & justification
  if (first + count > _numFrames) {
    error("ERROR: FileSource::loadFrames: requested frames [%u,%u), but only %u frames are available", first, first+count, _numFrames);
  }


  first += _startSkip + justificationOffset;

  if (first >= _minPastFrames) {
#if 0
    // I think the _min{Past,Future}Frames adjustment needs to apply to pre{First,Count}
    first -= _minPastFrames;
    count += _minPastFrames;
#endif
  } else {
    error("ERROR: FileSource::loadFrames: requested frames [%u,%u), but there must be %u frames before the first requested frame",
	  first, first+count, _minPastFrames);
  }
  if (first + count - 1 + _minFutureFrames <= _numCacheableFrames) {
#if 0
    // I think the _min{Past,Future}Frames adjustment needs to apply to pre{First,Count}
    count += _minFutureFrames;
#endif
  } else {
    error("ERROR: FileSource::loadFrames: requested frames [%u,%u), but there must be %u frames after the last requested frame",
	  first, first+count, _minFutureFrames);
  }

#if 0
printf("loadFrames [%u,%u) -> [%u,%u)  requires [%u,%u)\n", 
       requestedFirst, requestedFirst + requestedCount,
       first, first+count,
       first - _minPastFrames, first + count + _minPastFrames + _minFutureFrames);
#endif
  if (numBufferedFrames == 0) {
    // cache empty

    // FIXME - move this above if
    // also need to check against count + _minFutureFrames + _minPastFrames
    if (bufferFrames < count) {
      error("ERROR: FileSource::loadFrames: requested %u frames, but buffer can only hold %u", count, bufferFrames);
    }
    // FIXME - first + count + _minFutureFrames ?
    if (first + count > _numCacheableFrames) {
      error("ERROR: FileSource::loadFrames: requested frames %u to %u, but there are only %u frames available", first, first+count-1, _numFrames);
    }

    if (count + _minPastFrames + _minFutureFrames + 2 * window < bufferFrames) {
      preFirst = ( first > window + _minPastFrames ) ? first - window - _minPastFrames : 0;
      preCount = ( preFirst + count + _minPastFrames + _minFutureFrames + 2 * window < _numCacheableFrames ) ?
        count + _minPastFrames + _minFutureFrames + 2 * window : _numCacheableFrames - preFirst;
    } else {
      assert(first >= _minPastFrames);
      preFirst = first - _minPastFrames;
      preCount = count + _minPastFrames + _minFutureFrames;
      assert(preFirst + preCount <= _numCacheableFrames);
    }
    assert(preCount > 0);
    firstBufferedFrameIndex = (bufferFrames - preCount) / 2;
    firstBufferedFrame = preFirst;
    numBufferedFrames = preCount;
    if (firstBufferedFrameIndex + numBufferedFrames > bufferFrames) {
      error("ERROR: FileSource:loadFrames:  attempted to load %u frames at index %u, which overflows the frame buffer", numBufferedFrames, firstBufferedFrameIndex*bufStride);
    }
    Data32 const *frames = loadFrames(firstBufferedFrameIndex, preFirst, preCount);
    assert(frames == cookedBuffer + firstBufferedFrameIndex * bufStride);
#if 0
warning("CACHE EMPTY [%u,%u) -> [%u,%u)  cached [%u,%u) @ %u",
	requestedFirst, requestedFirst + count,
	first, first+count, 
	firstBufferedFrame, firstBufferedFrame+numBufferedFrames,
	(firstBufferedFrameIndex + (first - firstBufferedFrame))*bufStride);

for (unsigned x=preFirst; x < preFirst+preCount; x+=1) {
  unsigned buffOffset = firstBufferedFrameIndex * bufStride;
  printf("%3u:", x);
  for (unsigned c=0; c < numContinuous(); c+=1) printf(" %f", *((float    *)(cookedBuffer+buffOffset+c+x*bufStride)));
  for (unsigned d=0; d < numDiscrete()  ; d+=1) printf(" %u", *((unsigned *)(cookedBuffer+buffOffset+d+x*bufStride+numContinuous())));
  printf("  @ %u\n",buffOffset+x*bufStride );
}
#endif
#if 0
for (unsigned frm=0; frm < preCount; frm+=1) {
  printf("   C %u @ %u :", preFirst+frm, firstBufferedFrameIndex+frm);
  float *ppp = (float *)frames;
  for (unsigned j=0; j < 2; j+=1) {
    printf(" %f", *(ppp++));
  }
  printf("\n");
}
#endif
    return frames + (first - preFirst) * bufStride;
  }
  // FIXME - i think the -1 should be gone
  if (firstBufferedFrame + _minPastFrames <= first && 
      first + count - 1 + _minFutureFrames <= firstBufferedFrame + numBufferedFrames)
  { 
    // cache hit
    
    if (first - _minPastFrames - firstBufferedFrame <= delta && firstBufferedFrame > 0) {  // prefetch backward?
      preFirst = firstBufferedFrame > window ? firstBufferedFrame - window : 0;
      preCount = firstBufferedFrame - preFirst;
      if (firstBufferedFrameIndex + numBufferedFrames + preCount < bufferFrames) { // do prefetch
	firstBufferedFrame = preFirst;
	firstBufferedFrameIndex -= preCount;
#if 0
warning("CACHE HIT< [%u,%u)  cached [%u,%u)", first,first+count, firstBufferedFrame, firstBufferedFrame+numBufferedFrames);
#endif
#if 1
        (void)loadFrames(firstBufferedFrameIndex, preFirst, preCount);
#else
	Data32 const *frames = loadFrames(firstBufferedFrameIndex, preFirst, preCount);
#endif
	numBufferedFrames += preCount;
      } 
    } else if (firstBufferedFrame + numBufferedFrames - (first + count + _minFutureFrames) <= delta && 
	firstBufferedFrame + numBufferedFrames < _numCacheableFrames) 
    {  // prefetch forward?
      preFirst = firstBufferedFrame + numBufferedFrames;
      preCount = firstBufferedFrame + numBufferedFrames + window < _numCacheableFrames ? window : _numCacheableFrames - preFirst;
      if (firstBufferedFrameIndex + numBufferedFrames + preCount < bufferFrames) { // do prefetch
	// FIXME - does the above need a -1 ?
#if 0
warning("PREFETCH > [%u,%u)", preFirst, preFirst+preCount);
#endif
#if 1
        (void) loadFrames(firstBufferedFrameIndex+numBufferedFrames, preFirst, preCount);
#else
	Data32 const *frames = loadFrames(firstBufferedFrameIndex+numBufferedFrames, preFirst, preCount);
#endif
	numBufferedFrames += preCount;
#if 0
warning("CACHE HIT> [%u,%u)  cached [%u,%u)", first,first+count, firstBufferedFrame, firstBufferedFrame+numBufferedFrames);
#endif
	return cookedBuffer + (firstBufferedFrameIndex + (first - firstBufferedFrame)) * bufStride;
      } 
    } else {
    // no prefetch
#if 0
warning("CACHE HIT [%u,%u)  cached [%u,%u) @ %u", 
	first,first+count, 
	firstBufferedFrame, firstBufferedFrame+numBufferedFrames, 
	(firstBufferedFrameIndex + first - firstBufferedFrame)*bufStride);
#endif
    }
    assert(first - firstBufferedFrame >= _minPastFrames);
    return cookedBuffer + (firstBufferedFrameIndex + (first - firstBufferedFrame)) * bufStride;
  }

  // cache miss
#if 0
warning("CACHE MISS [%u,%u)  cached [%u,%u)", first,first+count, firstBufferedFrame, firstBufferedFrame+numBufferedFrames);
#endif
  if (count + _minPastFrames + _minFutureFrames + 2 * window < bufferFrames) {
    preFirst = ( first > window + _minPastFrames ) ? first - window - _minPastFrames : 0;
    preCount = ( preFirst + count + _minPastFrames + _minFutureFrames + 2 * window < _numCacheableFrames ) ?
      count + _minPastFrames + _minFutureFrames + 2 * window : _numCacheableFrames - preFirst;
  } else {
    assert(first >= _minPastFrames);
    preFirst = first - _minPastFrames;
    preCount = count + _minPastFrames + _minFutureFrames;
    assert(preFirst + preCount <= _numCacheableFrames);
  }
  firstBufferedFrameIndex = (bufferFrames - preCount) / 2;
  firstBufferedFrame = preFirst;
  numBufferedFrames = preCount;
#if 0
warning("CACHE FLUSH [%u,%u)  cached [%u,%u)", first, first+count, firstBufferedFrame, firstBufferedFrame+numBufferedFrames);
#endif
  Data32 const *frames = loadFrames(firstBufferedFrameIndex, preFirst, preCount);
  return frames + (first - preFirst) * bufStride;

#if 0
  if (bufferFrames < count) {
    error("ERROR: FileSource::loadFrames: requested %u frames, but buffer can only hold %u", count, bufferFrames);
  }
  firstBufferedFrameIndex = (bufferFrames - count) / 2;
  firstBufferedFrame = first;
  numBufferedFrames = count;
  return loadFrames(firstBufferedFrameIndex, first, count);
#endif
}


// FIXME - FileSource should know about feature subranges unless the file format can do it better
//  no it shouldn't - ObservationFile base class does naive feature subranges, subclasses
//  can over-ride the *Logical* methods if they can do better

// The number of continuous, discrete, total features
unsigned 
FileSource::numContinuous() {
  if (_numContinuous > -1) 
    return (unsigned) _numContinuous;

  unsigned numCont = 0, numDisc = 0;
  for (unsigned i=0; i < nFiles; i+=1) {
    if (ftrcombo == FTROP_NONE) {
      numCont += file[i]->numLogicalContinuous();
    } else {
      if (numCont < file[i]->numLogicalContinuous()) 
	numCont = file[i]->numLogicalContinuous();
    }
    numDisc += file[i]->numLogicalDiscrete();
  }
  subMatrixDescriptor wholeSegment(0U, 0U, 0U, 0U, numCont, numDisc, 0u);
  subMatrixDescriptor output = filter->describeOutput(wholeSegment);
  _numContinuous = (unsigned) output.numContinuous;
  _numDiscrete   = (unsigned) output.numDiscrete;
  return _numContinuous;
}

unsigned 
FileSource::numDiscrete() {
  if (_numDiscrete > -1) return (unsigned) _numDiscrete;
  (void) numContinuous();
  return _numDiscrete;
}

unsigned 
FileSource::numFeatures() {
  return numContinuous() + numDiscrete();
}

// The number of Data32's between each frame
unsigned 
FileSource::stride() {
  return numFeatures();
}



float *const 
FileSource::floatVecAtFrame(unsigned f) {
  assert(0 <= f && f < numFrames());
  Data32 const * buf = loadFrames(f, 1);
#if 0
  printf("%3u:", f);
  for (unsigned i=0; i < numContinuous(); i+=1) printf(" %f", ((float *)buf)[i]);
  for (unsigned i=numContinuous(); i < numFeatures(); i+=1) printf(" %u", ((unsigned *)buf)[i]);
#endif
  return(float *const) buf;
}

// FIXME - unsignedVecAtFrame() (both) need to add numContinuous()
unsigned *const 
FileSource::unsignedVecAtFrame(unsigned f) {
  assert(0 <=f && f < numFrames());
#if 0
unsigned *up = (unsigned *)(loadFrames(f,1)+_numContinuous);
printf("uVec(%3u):", f);
 for (unsigned i=0; i < (unsigned)_numDiscrete; i+=1) printf(" %u", up[i]);
printf("\n"); 
#endif
  return (unsigned *)loadFrames(f,1);
}

unsigned &
FileSource::unsignedAtFrame(const unsigned frame, const unsigned feature) {
  assert (0 <= frame && frame < numFrames());
  assert (feature >= numContinuous()
	  &&
	  feature <  numFeatures());
#if 0
unsigned *up = (unsigned *)(loadFrames(frame,1)+_numContinuous);
printf("uVec(%3u,%u):", frame, feature);
 for (unsigned i=0; i < (unsigned)_numDiscrete; i+=1) printf(" %u", up[i]);
printf("\n"); 
#endif
  return *(unsigned*)(loadFrames(frame,1)+feature);
}


float *const 
FileSource::floatVecAtFrame(unsigned f, const unsigned startFeature) {
  assert (0 <= f && f < numFrames());
  float *result = (float*)(loadFrames(f,1) + startFeature);
  return result;
}

Data32 const * const
FileSource::baseAtFrame(unsigned f) {
  assert(0 <= f && f < numFrames());
  Data32 const * featuresBase = loadFrames(f,1);
#if 0
printf("baseAtFrame(%3u / %3u):", f, f + _startSkip);
float *fp = (float *)(featuresBase);
for(unsigned i=0; i < (unsigned)_numContinuous; i+=1) printf(" %f", fp[i]);
unsigned *up = (unsigned *)(featuresBase);
for(unsigned i=_numContinuous; i < stride(); i+=1) printf(" %u", up[i]);
printf("\n");
#endif
  return featuresBase;
}

bool 
FileSource::elementIsDiscrete(unsigned el) {
  return numContinuous() <= el && el < numFeatures();
}

bool
FileSource::active() {
  return this->segment > -1;
}