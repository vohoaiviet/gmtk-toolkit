/*
 * gmtkDMLPtrain.cc
 *
 * Written by Richard Rogers <rprogers@ee.washington.edu>
 *
 * Copyright (C) 2013 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 *
 */


#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <float.h>
#include <assert.h>


#include "DBN.h"
#include "MMapMatrix.h"

#include "general.h"
#include "error.h"
#include "rand.h"
#include "arguments.h"
#include "ieeeFPsetup.h"
#include "debug.h"
//#include "spi.h"
#include "version.h"

#if HAVE_CONFIG_H
#include <config.h>
#endif
#if HAVE_HG_H
#include "hgstamp.h"
#endif
VCID(HGID)


#include "GMTK_FileParser.h"
#include "GMTK_RV.h"
#include "GMTK_DiscRV.h"
#include "GMTK_ContRV.h"
#include "GMTK_GMParms.h"
#include "GMTK_GMTemplate.h"
#include "GMTK_Partition.h"

#include "GMTK_ObservationSource.h"
#include "GMTK_FileSource.h"
#include "GMTK_CreateFileSource.h"
#include "GMTK_ASCIIFile.h"
#include "GMTK_FlatASCIIFile.h"
#include "GMTK_PFileFile.h"
#include "GMTK_HTKFile.h"
#include "GMTK_HDF5File.h"
#include "GMTK_BinaryFile.h"
#include "GMTK_Filter.h"
#include "GMTK_Stream.h"
#include "GMTK_RandomSampleSchedule.h"

#include "GMTK_MixtureCommon.h"
#include "GMTK_GaussianComponent.h"
#include "GMTK_LinMeanCondDiagGaussian.h"
#include "GMTK_MeanVector.h"
#include "GMTK_DiagCovarVector.h"
#include "GMTK_DlinkMatrix.h"

#include "GMTK_WordOrganization.h"

#include "GMTK_DeepVECPT.h"

#define GMTK_ARG_OBS_FILES
/****************************      FILE RANGE OPTIONS             ***********************************************/
#define GMTK_ARG_FILE_RANGE_OPTIONS
#define GMTK_ARG_TRRNG
#define GMTK_ARG_START_END_SKIP
/************************  OBSERVATION MATRIX TRANSFORMATION OPTIONS   ******************************************/
#define GMTK_ARG_OBS_MATRIX_OPTIONS
#define GMTK_ARG_OBS_MATRIX_XFORMATION


/*************************   INPUT TRAINABLE PARAMETER FILE HANDLING  *******************************************/
#define GMTK_ARG_INPUT_TRAINABLE_FILE_HANDLING
#define GMTK_ARG_INPUT_MASTER_FILE_OPT_ARG

#define GMTK_ARG_OUTPUT_MASTER_FILE
#define GMTK_ARG_OUTPUT_TRAINABLE_PARAMS
#define GMTK_ARG_INPUT_TRAINABLE_PARAMS
#define GMTK_ARG_CPP_CMD_OPTS
#define GMTK_ARG_ALLOC_DENSE_CPTS
#define GMTK_ARG_CPT_NORM_THRES


/*************************   CONTINUOUS RANDOM VARIABLE OPTIONS       *******************************************/
#define GMTK_ARG_CONTINUOUS_RANDOM_VAR_OPTIONS
#define GMTK_ARG_VAR_FLOOR
#define GMTK_ARG_VAR_FLOOR_ON_READ


/*************************   DEEP MLP TRAINING OPTIONS                *******************************************/

#define GMTK_ARG_DMLP_TRAINING_OPTIONS
#define GMTK_ARG_DMLP_TRAINING_PARAMS


/*************************   GENERAL OPTIONS                          *******************************************/

#define GMTK_ARG_GENERAL_OPTIONS
#define GMTK_ARG_SEED
#define GMTK_ARG_VERB
#define GMTK_ARG_VERSION
#define GMTK_ARG_HELP


#define GMTK_ARGUMENTS_DEFINITION
#include "GMTK_Arguments.h"
#undef GMTK_ARGUMENTS_DEFINITION


Arg Arg::Args[] = {


#define GMTK_ARGUMENTS_DOCUMENTATION
#include "GMTK_Arguments.h"
#undef GMTK_ARGUMENTS_DOCUMENTATION



  // final one to signal the end of the list
  Arg()

};

/*
 * definition of needed global arguments
 */
RAND rnd(false);
GMParms GM_Parms;
#if 0
ObservationMatrix globalObservationMatrix;
#endif

FileSource *gomFS;
ObservationSource *globalObservationMatrix;

#include <iostream>

int
main(int argc,char*argv[])
{
  ////////////////////////////////////////////
  // set things up so that if an FP exception
  // occurs such as an "invalid" (NaN), overflow
  // or divide by zero, we actually get a FPE
  ieeeFPsetup();

  CODE_TO_COMPUTE_ENDIAN;

  ////////////////////////////////////////////
  // parse arguments
  bool parse_was_ok = Arg::parse(argc,(char**)argv,
"\nThis program prints out some information about the number of variables\n"
"in a model and which GMTK features the model uses\n");
  if(!parse_was_ok) {
    Arg::usage(); exit(-1);
  }


#define GMTK_ARGUMENTS_CHECK_ARGS
#include "GMTK_Arguments.h"
#undef GMTK_ARGUMENTS_CHECK_ARGS


  /////////////////////////////////////////////


  infoMsg(IM::Max,"Opening Files ...\n");
  gomFS = instantiateFileSource();
  globalObservationMatrix = gomFS;
  infoMsg(IM::Max,"Finished opening files.\n");

  if (inputMasterFile != NULL) {
    iDataStreamFile pf(inputMasterFile,false,true,cppCommandOptions);
    GM_Parms.read(pf);
  }
  if (inputTrainableParameters != NULL) {
    // flat, where everything is contained in one file
    iDataStreamFile pf(inputTrainableParameters,binInputTrainableParameters,true,cppCommandOptions);
    GM_Parms.readTrainable(pf);
  }
  GM_Parms.finalizeParameters();  
  
  printf("Finished reading in all parameters and structures\n");

  gomFS->openSegment(0);

  string DVECPTNameStr(DVECPTName);
  if (GM_Parms.deepVECptsMap.find(DVECPTNameStr) == GM_Parms.deepVECptsMap.end()) {
    error("Error: No Deep VE CPT named '%s' found\n", DVECPTName);
  }
  DeepVECPT *cpt = GM_Parms.deepVECpts[ GM_Parms.deepVECptsMap[DVECPTNameStr] ];

  printf("Total number of trainable parameters in input files = %u\n",
	 cpt->totalNumberDMLPParameters());

  struct rusage rus; /* starting time */
  struct rusage rue; /* ending time */
  getrusage(RUSAGE_SELF,&rus);

  int inputSize =  (int)cpt->getDeepNN()->numInputs();
  int numLayers =  (int)cpt->getDeepNN()->numLayers();
  int outputSize = (int)cpt->getDeepNN()->numOutputs();
  
  vector<int> hiddenSize(numLayers);
  for (unsigned i=0; i < numLayers; i+=1)
    hiddenSize[i] = (int)cpt->getDeepNN()->layerOutputs(i);

  vector<Layer::ActFunc> hActFunc(numLayers);
  for (unsigned i=0; i < numLayers; i+=1) {
    switch (cpt->getDeepNN()->getSquashFn(i)) {
    case DeepNN::SOFTMAX: 
      if (i != numLayers - 1) {
	error("ERROR: gmtkDMLPtrain only supports softmax for the output layer\n");
      }
      hActFunc[i] = Layer::ActFunc(Layer::ActFunc::LINEAR);
      break;
    case DeepNN::LOGISTIC: 
      if (i == numLayers - 1) {
	error("ERROR: gmtkDMLPtrain only supports linear or softmax for the output layer\n");
      }
      hActFunc[i] = Layer::ActFunc(Layer::ActFunc::LOG_SIG); 
      break;
    case DeepNN::TANH: 
      if (i == numLayers - 1) {
	error("ERROR: gmtkDMLPtrain only supports linear or softmax for the output layer\n");
      }
      hActFunc[i] = Layer::ActFunc(Layer::ActFunc::TANH); 
      break;
    case DeepNN::ODDROOT: 
      if (i == numLayers - 1) {
	error("ERROR: gmtkDMLPtrain only supports linear or softmax for the output layer\n");
      }
      hActFunc[i] = Layer::ActFunc(Layer::ActFunc::CUBIC); 
      break;
    case DeepNN::LINEAR:
      hActFunc[i] = Layer::ActFunc(Layer::ActFunc::LINEAR);
      break;
    case DeepNN::RECTLIN:
      if (i == numLayers - 1) {
	error("ERROR: gmtkDMLPtrain only supports linear or softmax for the output layer\n");
      }
      if (pretrainMode != DBN::NONE) {
	error("ERROR: gmtkDMLPtrain only supports rectified linear activation functions with -pretrainType none\n");
      }
      hActFunc[i] = Layer::ActFunc(Layer::ActFunc::RECT_LIN);
      break;
    default: 
      error("Error: unknown activation function\n");
    }
  }

  vector<AllocatingMatrix> W(numLayers);
  vector<AllocatingVector> B(numLayers);
  for (unsigned j=0; j < numLayers; j+=1) {
    double *params;
    int rows, cols;
    cpt->getDeepNN()->getParams(j, rows, cols, params);
    Matrix P(params, cols, rows, cols, false);
    W[j].CopyFrom( P.SubMatrix(0, cols-1, 0, rows) ); // -1 for bias column
    B[j].CopyFrom( P.GetRow(cols - 1) );
  }

  DBN dbn(numLayers, inputSize, hiddenSize, outputSize, iActFunc, hActFunc, W, B);

  vector<DBN::HyperParams> pretrainHyperParams(numLayers);
  for (int j = 0; j < numLayers; j+=1) {
    pretrainHyperParams[j].initStepSize     = ptInitStepSize;
    pretrainHyperParams[j].maxMomentum      = ptMaxMomentum;
    pretrainHyperParams[j].maxUpdate        = ptMaxUpdate;
    pretrainHyperParams[j].l2               = ptL2;
    pretrainHyperParams[j].numUpdates       = ptNumUpdates;
    pretrainHyperParams[j].numAnnealUpdates = ptNumAnnealUpdates;
    pretrainHyperParams[j].miniBatchSize    = ptMiniBatchSize;
    pretrainHyperParams[j].checkInterval    = ptCheckInterval;
    pretrainHyperParams[j].dropout          = ptDropout;
    pretrainHyperParams[j].pretrainType     = pretrainMode;
  }

  DBN::HyperParams bpHyperParams;
  bpHyperParams.initStepSize     = bpInitStepSize;
  bpHyperParams.maxMomentum      = bpMaxMomentum;
  bpHyperParams.maxUpdate        = bpMaxUpdate;
  bpHyperParams.l2               = bpL2;
  bpHyperParams.numUpdates       = bpNumUpdates;
  bpHyperParams.numAnnealUpdates = bpNumAnnealUpdates;
  bpHyperParams.miniBatchSize    = bpMiniBatchSize;
  bpHyperParams.checkInterval    = bpCheckInterval;
  bpHyperParams.dropout          = bpDropout;

  unsigned radius = cpt->windowRadius();
  gomFS->setMinPastFrames( radius );
  gomFS->setMinFutureFrames( radius );

  unsigned obsOffset = cpt->obsOffset();
  unsigned numFeaturesPerFrame = cpt->numFeaturesPerFrame();

  if (oneHot) {
    if (labelOffset < gomFS->numContinuous()) {
      error("ERROR: labelOffset (%u) must refer to a discrete feature (the first %u are continuous)\n", 
	    labelOffset, gomFS->numContinuous());
    }
    if (labelOffset >= gomFS->numFeatures()) {
      error("ERROR: labelOffset (%u) is too large for the number of available features (%u)\n",
	    labelOffset, gomFS->numFeatures());
    }
  } else {
    if (labelOffset >= gomFS->numContinuous()) {
      error("ERROR: labelOffset (%u) is too large for the number of continuous features (%u)\n",
	    labelOffset, gomFS->numContinuous());
    }
    if (labelOffset + outputSize > gomFS->numContinuous()) {
      error("ERROR: labelOffset (%u) + number of outputs (%u) is too large for the number of continuous features (%u)\n", 
	    labelOffset, outputSize, gomFS->numContinuous());
    }
  }
  RandomSampleSchedule rss(obsOffset, numFeaturesPerFrame,
			   labelOffset, outputSize, oneHot, radius, 
			   1, gomFS, trrng_str);


  unsigned numUnits = rss.numTrainingUnits();

  unsigned features_per_instance, instances_per_unit, dataSize, labelSize, labelStride;
  rss.describeFeatures(features_per_instance, instances_per_unit);
  dataSize = features_per_instance * instances_per_unit;
  double *ddata = new double[dataSize];

  rss.describeLabels(features_per_instance, instances_per_unit, labelStride);
  labelSize = instances_per_unit * labelStride;
  double *dlabel = new double[labelSize];

  MMapMatrix   trainData(inputSize,  numUnits, inputSize);
  MMapMatrix trainLabels(outputSize, numUnits, outputSize);

  unsigned segment, frame, destCol = 0;
  for (unsigned b=0; b < numUnits && rss.nextTrainingUnit(segment, frame); b+=1) {

    float *data = rss.getFeatures(segment, frame);
    for (unsigned i=0; i < dataSize; i+=1) ddata[i] = (double) data[i];
    trainData.PutCols(ddata, 1, inputSize, inputSize, destCol);

    data = rss.getLabels(segment, frame);
    for (unsigned i=0; i < labelSize; i+=1) dlabel[i] = (double) data[i];
    trainLabels.PutCols(dlabel, 1, outputSize, labelStride, destCol);

    destCol += 1;
  }
  delete[] ddata;
  delete[] dlabel;

  DBN::ObjectiveType objType = 
    ( cpt->getDeepNN()->getSquashFn(numLayers-1) == DeepNN::SOFTMAX ) ? DBN::SOFT_MAX : DBN::SQ_ERR;

  dbn.Train(trainData, trainLabels, objType, false, pretrainHyperParams, bpHyperParams, resumeTraining);

  vector<DoubleMatrix *> layerMatrix = cpt->getDeepNN()->getMatrices();
  assert(layerMatrix.size() == numLayers);
  for (unsigned layer=0; layer < numLayers; layer+=1) {
    Matrix const W = dbn.getWeights(layer);
    cpt->getDeepNN()->setParams(layer, W.Start(), W.Ld(), dbn.getBias(layer).Start());
  }
  
  if (outputTrainableParameters != NULL) {
    char buff[2048];
    copyStringWithTag(buff,outputTrainableParameters,
		      CSWT_EMPTY_TAG,2048);
    oDataStreamFile of(buff,binOutputTrainableParameters);
    GM_Parms.writeTrainable(of);
  }

  // also write according to output master
  GM_Parms.write(outputMasterFile,cppCommandOptions);  

  getrusage(RUSAGE_SELF,&rue);
  if (IM::messageGlb(IM::Default)) { 
    infoMsg(IM::Default,"### Final time (seconds) just for DMLP training stage: ");
    double userTime,sysTime;
    reportTiming(rus,rue,userTime,sysTime,stdout);
  }
  MMapMatrix::GarbageCollect(); // delete left-over temp files
  exit_program_with_status(0);
}

