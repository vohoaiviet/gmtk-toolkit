
#ifndef GMTK_TEMPORARY_H
#define GMTK_TEMPORARY_H

/*
 *
 * Copyright (C) 2012 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 */

#define INITIAL_NUM_FRAMES_IN_BUFFER 1000

#define NONE_LETTER 'X'
#define TRANS_NORMALIZATION_LETTER 'N'
#define TRANS_MEAN_SUB_LETTER 'E'
#define TRANS_UPSAMPLING_LETTER 'U'
#define TRANS_HOLD_LETTER 'H'
#define TRANS_SMOOTH_LETTER 'S'
#define TRANS_MULTIPLICATION_LETTER 'M'
#define TRANS_ARMA_LETTER 'R'
#define TRANS_OFFSET_LETTER 'O'
#define FILTER_LETTER 'F'
#define END_STR (-1)
#define UNRECOGNIZED_TRANSFORM (-2)
#define SEPARATOR "_"

#define MAX_NUM_STORED_FILTERS 10
#define MAX_FILTER_LEN 100
#define MIN_FILTER_LEN 1

#define ALLOW_VARIABLE_DIM_COMBINED_STREAMS 1

#ifdef DEBUG
#define DBGFPRINTF(_x_) fprintf _x_
#else
#define DBGFPRINTF(_x_)
#endif


// transformations
enum {
  NONE,
  UPSAMPLE_HOLD,
  UPSAMPLE_SMOOTH,
  DOWNSAMPLE,
  DELTAS,
  DOUBLE_DELTAS,
  MULTIPLY,
  NORMALIZE,
  MEAN_SUB,
  ARMA,
  FILTER,
  OFFSET
};

enum {
  FRAMEMATCH_ERROR,
  FRAMEMATCH_REPEAT_LAST,
  FRAMEMATCH_REPEAT_FIRST,
  FRAMEMATCH_EXPAND_SEGMENTALLY,
  FRAMEMATCH_TRUNCATE_FROM_END,
  FRAMEMATCH_TRUNCATE_FROM_START,
  FRAMEMATCH_REPEAT_RANGE_AT_START, // not used yet
  FRAMEMATCH_REPEAT_RANGE_AT_END   // not used yet
};

enum {
  SEGMATCH_ERROR,
  SEGMATCH_TRUNCATE_FROM_END,
  SEGMATCH_REPEAT_LAST,
  SEGMATCH_WRAP_AROUND
};

// ftrcombo operation flags
enum {
  FTROP_NONE = 0,
  FTROP_ADD,
  FTROP_SUB,
  FTROP_MUL,
  FTROP_DIV
  };

#endif


