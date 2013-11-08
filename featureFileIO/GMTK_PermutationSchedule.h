/*
 * GMTK_PermutationSchedule.h
 * 
 * Written by Richard Rogers <rprogers@ee.washington.edu>
 *
 * Copyright (C) 2013 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 * See COPYING or http://opensource.org/licenses/OSL-3.0
 *
 */

#ifndef GMTK_PERMUTATIONSCHEDULE_H
#define GMTK_PERMUTATIONSCHEDULE_H

#include <string.h>

#include "GMTK_TrainingSchedule.h"
#include "GMTK_ObservationSource.h"
#include "rand.h"
#include "prime.h"
#include "error.h"
#include "debug.h"


// Return non-overlapping training units of requested size in observation source order.
// If a segment's length is not a multiple of the unit size, the last unit from that
// segment will be short.

class PermutationSchedule : public TrainingSchedule {

  unsigned *segmentPerm;     // segment # of training unit permutation
  unsigned *framePerm;       // frame # of training unit permutation
  unsigned  i;               // position in permutation

  unsigned  a, b, p;         // p is the smallest prime \equiv_3 2 larger than num_units
                             // a is an integer in [1, p)  and b is an integer in [0, p)
                             // \sigma(i) = (ai+b)^3 mod p is a permutation of 0, 1, ..., p-1
 public:

  PermutationSchedule(unsigned feature_offset, unsigned features_per_frame,
		 unsigned label_offset,  unsigned label_domain_size,
		 bool one_hot, unsigned window_radius, unsigned unit_size, 
		 FileSource *obs_source, char const *trrng_str)
    : TrainingSchedule(feature_offset, features_per_frame, label_offset, 
		       label_domain_size, one_hot, window_radius, unit_size,
		       obs_source, trrng_str), i(0)
  {
    // count viable training units
    num_viable_units = 0;
    Range::iterator* trrng_it = new Range::iterator(trrng->begin());
    while (!trrng_it->at_end()) {
      unsigned i = (unsigned)(*(*trrng_it));
      if (!obs_source->openSegment(i)) {
	error("ERROR: Unable to open observation file segment %u\n", i);
      }
      unsigned num_frames = obs_source->numFrames();
      unsigned units = num_frames / unit_size;
      if (num_frames % unit_size) units += 1;
      if (units == 0) {
	warning("WARNING: segment %u contains no frames\n", i);
      }
      num_viable_units += units;
      (*trrng_it)++;
    }
    if (num_viable_units == 0) {
      error("ERROR: observation files contain no viable training instances\n");
    }

    // find smallest prime >= num_viable_units that is \equiv_3 2
    for (p = num_viable_units + (2 - num_viable_units % 3); !prime32(p); p += 3)
      ;
    a = rnd.uniform(1, p-1);     // pick a random a in [1,p)
    b = rnd.uniform(p-1);        // pick a random b in [0,p)
    infoMsg(IM::ObsFile, IM::Moderate, "T=%u <= %u = %u mod 3;  sigma(i) = (%u i + %u)^3 mod %u\n",
	    num_viable_units, p, p%3, a, b, p);
    // initialize segment & frame permutation arrays 
    segmentPerm = new unsigned[num_viable_units];
    framePerm = new unsigned[num_viable_units];
    trrng_it->reset();
    unsigned j=0;
    while (!trrng_it->at_end()) {
      unsigned i = (unsigned)(*(*trrng_it));
      if (!obs_source->openSegment(i)) {
	error("ERROR: Unable to open observation file segment %u\n", i);
      }
      unsigned num_frames = obs_source->numFrames();
      unsigned units = num_frames / unit_size;
      if (num_frames % unit_size) units += 1;
      for (unsigned u = 0, f=0; u < units; u+=1, f+=unit_size, j+=1) {
	segmentPerm[j] = i;
	framePerm[j] = f;
      }
      (*trrng_it)++;
    }
  } 

  
  ~PermutationSchedule() {
    delete[] segmentPerm;
    delete[] framePerm;
  }

  
  void nextTrainingUnit(unsigned &segment, unsigned &frame) { 
    unsigned sigma; // sigma(i) = (ai + b)^3 mod p
    do {
      unsigned long t = (a * i) % p;
      t = (t + b) % p;
      unsigned long tt = (t * t) % p;
      sigma = (t * tt) % p;
      i = (i + 1) % p;
    } while (sigma >= num_viable_units); // skip any extras since p >= num_viable_units

    segment = segmentPerm[sigma];
    frame = framePerm[sigma];
    TrainingSchedule::nextTrainingUnit(segment, frame);
  }
};

#endif
