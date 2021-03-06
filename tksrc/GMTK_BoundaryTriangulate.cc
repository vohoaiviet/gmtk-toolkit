/*
 * GMTK_BoundaryTriangulate.cc
 *   The GMTK Boundary Algorithm and Graph Triangulation Support Routines
 *
 * Written by Jeff Bilmes <bilmes@ee.washington.edu> & Chris Bartels <bartels@ee.washington.edu>
 *
 * Copyright (C) 2003 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 * See COPYING or http://opensource.org/licenses/OSL-3.0
 *
 *
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <map>
#include <set>

#include "debug.h"
#include "error.h"
#include "general.h"
#include "rand.h"

#include "GMTK_BoundaryTriangulate.h"
#include "GMTK_DiscRV.h"
#include "GMTK_FileParser.h"
#include "GMTK_MDCPT.h"
#include "GMTK_Mixture.h"
#include "GMTK_GMParms.h"
#include "GMTK_MSCPT.h"
#include "GMTK_MTCPT.h"
#if 0
#include "GMTK_ObservationMatrix.h"
#endif
#include "GMTK_JunctionTree.h"
#include "GMTK_GraphicalModel.h"
#include "GMTK_NetworkFlowTriangulate.h"
#include "GMTK_CountIterator.h"

#if HAVE_CONFIG_H
#include <config.h>
#endif
#if HAVE_HG_H
#include "hgstamp.h"
#endif
VCID(HGID)


#ifndef MAXFLOAT
#define MAXFLOAT (3.40282347e+38F)
#endif

/*******************************************************************************
 *******************************************************************************
 *******************************************************************************
 **
 **           General Support Routines
 **
 *******************************************************************************
 *******************************************************************************
 *******************************************************************************
 */


/*-
 *-----------------------------------------------------------------------
 * Partition::saveCurrentNeighbors()
 *   Save a copy of the current neighbors of the partition.
 *
 * Preconditions:
 *   The corresponding partition must be instantiated.
 *
 * Postconditions:
 *   a copy of the current neighborset will be made
 *
 * Side Effects:
 *   Any previous neighbor set will be lost.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
saveCurrentNeighbors(const set<RV*> nodes,SavedGraph& orgnl_nghbrs)
{
  set<RV*>::iterator crrnt_node; 
  set<RV*>::iterator end_node; 
  orgnl_nghbrs.clear();
  for ( crrnt_node=nodes.begin(), end_node=nodes.end();
        crrnt_node != end_node;
        ++crrnt_node ) {
    orgnl_nghbrs.push_back(
      NghbrPairType((*crrnt_node), (*crrnt_node)->neighbors) );
  }
}


/*-
 *-----------------------------------------------------------------------
 * Partition::saveCurrentNeighbors()
 *   Save a copy of the current neighbors of the partition.
 *
 * Preconditions:
 *   The corresponding partition must be instantiated.
 *
 * Postconditions:
 *   a copy of the current neighborset will be made
 *
 * Side Effects:
 *   Any previous neighbor set will be lost.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
saveCurrentNeighbors(
  vector<triangulateNode>& nodes,
  vector<triangulateNghbrPairType>& orgnl_nghbrs
  )
{
  vector<triangulateNode>::iterator crrnt_node; 
  vector<triangulateNode>::iterator end_node; 

  orgnl_nghbrs.clear();
  for ( crrnt_node=nodes.begin(), end_node=nodes.end();
        crrnt_node != end_node;
        ++crrnt_node ) {
    orgnl_nghbrs.push_back(
      triangulateNghbrPairType(&(*crrnt_node), (*crrnt_node).neighbors) );
  }
}


/*-
 *-----------------------------------------------------------------------
 * Partition::restoreNeighbors()
 *   Restore graph's neighbors to state on previous save.
 *   This can be used as an "unTriangulate" routine.
 *
 * Preconditions:
 *   The corresponding partition must be instantiated. 
 *   For this routine to have any effect, previous neighbors must have 
 *     already been saved.
 *
 * Postconditions:
 *   Old neighbors restored.
 *
 * Side Effects:
 *   Any previous neighbor set will be lost.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
restoreNeighbors(SavedGraph& orgnl_nghbrs)
{
  vector<pair<RV*, set<RV*> > >::iterator crrnt_node;
  vector<pair<RV*, set<RV*> > >::iterator end_node;
  for( end_node=orgnl_nghbrs.end(), crrnt_node=orgnl_nghbrs.begin();
       crrnt_node!=end_node;
       ++crrnt_node )
  {
    (*crrnt_node).first->neighbors = (*crrnt_node).second;
  }
}


/*-
 *-----------------------------------------------------------------------
 * Partition::restoreNeighbors()
 *   Restore graph's neighbors to state on previous save.
 *   This can be used as an "unTriangulate" routine.
 *
 * Preconditions:
 *   The corresponding partition must be instantiated. 
 *   For this routine to have any effect, previous neighbors must have 
 *     already been saved.
 *
 * Postconditions:
 *   Old neighbors restored.
 *
 * Side Effects:
 *   Any previous neighbor set will be lost.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
restoreNeighbors(vector<triangulateNghbrPairType>& orgnl_nghbrs)
{
  vector<pair<triangulateNode*, vector<triangulateNode*> > >::iterator 
    crrnt_node;
  vector<pair<triangulateNode*, vector<triangulateNode*> > >::iterator  
    end_node;

  for( end_node=orgnl_nghbrs.end(), crrnt_node=orgnl_nghbrs.begin();
       crrnt_node!=end_node;
       ++crrnt_node )
  {
    (*crrnt_node).first->neighbors = (*crrnt_node).second;
  }
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::deleteNodes()
 *   Given a set of random variables, delete the nodes pointed to by
 *   the set.
 *
 * Preconditions:
 *   'nodes' is a valid set of node pointers
 *
 * Postconditions:
 *   all RV*'s in 'nodes' have been deleted. The set should thereafter
 *   immediately be deleted or filled with new nodes
 *
 * Side Effects:
 *     Will delete all variables pointed to by the RV*'s within the set.
 *
 * Results:
 *     none
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
deleteNodes(const set<RV*>& nodes)
{
  for (set<RV*>::iterator i = nodes.begin();
       i != nodes.end(); i++) 
    delete (*i);
}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::parseTriHeuristicString()
 *      parses a triheuristic string returning a triHeuristic structure
 *
 * Preconditions:
 *      - String should contain valid tri heuristic, see code
 *        for definition
 *
 * Postconditions:
 *      - structure has been filled in.
 *
 * Side Effects:
 *      none
 *
 * Results:
 *     filled in vector via argument
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::parseTriHeuristicString(const string& tri_heur_str,
					     TriangulateHeuristics& tri_heur)
{
  const char pre_edge_all[]    = "pre-edge-all-";
  const char pre_edge_lo[]     = "pre-edge-lo-";
  const char pre_edge_random[] = "pre-edge-random-";
  const char pre_edge_some[]   = "pre-edge-some-";
  const char complete_frame_left [] = "completeframeleft";
  const char complete_frame_right [] = "completeframeright";

  tri_heur.init();
  if (tri_heur_str.size() == 0) {
    return;
  } else {

    ////////////////////////////////////////////////////////////////////////// 
    // Parse the prefix
    ////////////////////////////////////////////////////////////////////////// 
    char *endp;
    long number_1 = -1;
    long number_2 = -1;
    long number_3 = -1;
    const char *startp = tri_heur_str.c_str();

    tri_heur.seconds      = -1;
    tri_heur.numberTrials = -1;

    number_1 = strtol(startp, &endp, 10);

    if (endp != startp) {
      ////////////////////////////////////////////////////////////////// 
      // If string begins with 'N-' then N is number of iterations 
      ////////////////////////////////////////////////////////////////// 
      if (*endp == '-') {
        tri_heur.numberTrials = number_1;
        endp++;
      }
      ////////////////////////////////////////////////////////////////// 
      // If string is 'm:s' or 'h:m:s' then number is a time string 
      ////////////////////////////////////////////////////////////////// 
      else if (*endp == ':') {
        endp++;
        number_2 = strtol(endp, &endp, 10);
        
        ////////////////////////////////////////////////////////////////// 
        // Separate cases for minutes:seconds and hours:minutes:seconds 
        ////////////////////////////////////////////////////////////////// 
        if (*endp == ':') {
          endp++;
          number_3 = strtol(endp, &endp, 10);
          tri_heur.seconds = 60*60*number_1 + 60*number_2 + number_3;
          endp++;
        }
        else {
          tri_heur.seconds = 60*number_1 + number_2; 
          endp++;
        }

      }
    }
    else {
      tri_heur.numberTrials = 1;
    }

    if ( (endp == NULL) || 
         ((tri_heur.numberTrials < 0) && (tri_heur.seconds < 0)) ) {
      error("ERROR: bad triangulation heuristic string given in '%s'\n",startp);
    }

    ////////////////////////////////////////////////////////////////////////// 
    // Parse the heuristic type 
    ////////////////////////////////////////////////////////////////////////// 
    if (strncmp(endp, "anneal", strlen(endp)) == 0) {
      tri_heur.style = TS_ANNEALING;
    } else if (strncmp(endp, "exhaustive", strlen(endp)) == 0) {
      tri_heur.style = TS_EXHAUSTIVE;
    } else if (strncmp(endp, "random", strlen(endp)) == 0) {
      tri_heur.style = TS_RANDOM;
    } else if (strncmp(endp, pre_edge_all, strlen(pre_edge_all)) == 0) {
      tri_heur.style = TS_PRE_EDGE_ALL;
      tri_heur.basic_method_string = &endp[strlen(pre_edge_all)];
    } else if (strncmp(endp, pre_edge_lo, strlen(pre_edge_lo)) == 0) {
      tri_heur.style = TS_PRE_EDGE_LO;
      tri_heur.basic_method_string = &endp[strlen(pre_edge_lo)];
    } else if (strncmp(endp, pre_edge_random, strlen(pre_edge_random)) == 0) {
      tri_heur.style = TS_PRE_EDGE_RANDOM;
      tri_heur.basic_method_string = &endp[strlen(pre_edge_random)];
    } else if (strncmp(endp, pre_edge_some, strlen(pre_edge_some)) == 0) {
      tri_heur.style = TS_PRE_EDGE_SOME;
      tri_heur.basic_method_string = &endp[strlen(pre_edge_some)];
    } else if (strncmp(endp, "heuristics", strlen(endp)) == 0) {
      tri_heur.style = TS_ALL_HEURISTICS;
    } else if (strncmp(endp, "elimination-heuristics", strlen(endp)) == 0) {
      tri_heur.style = TS_ELIMINATION_HEURISTICS;
    } else if (strncmp(endp, "non-elimination-heuristics", strlen(endp)) == 0) {
      tri_heur.style = TS_NON_ELIMINATION_HEURISTICS;
    } else if (strncmp(endp, "frontier", strlen(endp)) == 0) {
      tri_heur.style = TS_FRONTIER;
    } else if (strcmp(endp, "MCS") == 0) { 
      // MCS must be specified with full "MCS" string, so use strcmp
      // rather than strncmp here.
      tri_heur.style = TS_MCS;
    } else if (strncmp(endp, "completed", strlen(endp)) == 0) {
      tri_heur.style = TS_COMPLETED;
    } else if (strncmp(endp, complete_frame_left, strlen(complete_frame_left)) == 0) {
      tri_heur.style = TS_COMPLETE_FRAME_LEFT;
      tri_heur.basic_method_string = &endp[strlen(complete_frame_left)];
    } else if (strncmp(endp, complete_frame_right, strlen(complete_frame_right)) == 0) {
      tri_heur.style = TS_COMPLETE_FRAME_RIGHT;
      tri_heur.basic_method_string = &endp[strlen(complete_frame_right)];
    } else {
      tri_heur.style = TS_BASIC;
      tri_heur.heuristic_vector.clear();
      tri_heur.basic_method_string = string(endp);
      while (*endp) {
	switch (*endp) {
	case 'S':
	  tri_heur.heuristic_vector.push_back(TH_MIN_SIZE);
	  break;
	case 'T':
	  tri_heur.heuristic_vector.push_back(TH_MIN_TIMEFRAME);
	  break;
	case 'X':
	  tri_heur.heuristic_vector.push_back(TH_MAX_TIMEFRAME);
	  break;
	case 'F':
	  tri_heur.heuristic_vector.push_back(TH_MIN_FILLIN);
	  break;
	case 'I':
	  tri_heur.heuristic_vector.push_back(TH_WEIGHTED_MIN_FILLIN);
	  break;
	case 'W':
	  tri_heur.heuristic_vector.push_back(TH_MIN_WEIGHT);
	  break;
	case 'E':
	  tri_heur.heuristic_vector.push_back(TH_MIN_ENTROPY);
	  break;
	case 'P':
	  tri_heur.heuristic_vector.push_back(TH_MIN_POSITION_IN_FILE);
	  break;
	case 'H':
	  tri_heur.heuristic_vector.push_back(TH_MIN_HINT);
	  break;
	case 'N':
	  tri_heur.heuristic_vector.push_back(TH_MIN_WEIGHT_NO_D);
	  break;
	case 'R':
	  tri_heur.heuristic_vector.push_back(TH_RANDOM);
	  break;
	default:
	  {
	    // next check if there is a number at the end to determine
	    // numRandomTop for a basic heuristic.
	    char *end_endp;
	    // skip optional '-' character.
	    if (*endp == '-') 
	      endp++;
	    unsigned tmpu = strtol(endp,&end_endp,10);
	    if (endp != end_endp) {
	      // then there is a num random top heuristic number.
	      tri_heur.numRandomTop = tmpu;
	      endp = end_endp - 1;
	    } else
	      error("ERROR: Unknown triangulation heuristic given starting at position '%s' in string '%s'\n",
		    endp,tri_heur_str.c_str());
	  }
	  break;
	}
	endp++;
      }
    }
  }
  return;
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::createVectorBoundaryHeuristic()
 *      create a vector of prioritized boundary heuristics based
 *      on a string that is passed in.
 *
 * Preconditions:
 *      - String should contain set of valid heuristcs, see code
 *        for definition
 *
 * Postconditions:
 *      - vector has been filled in.
 *
 * Side Effects:
 *      none
 *
 * Results:
 *     filled in vector via argument
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::createVectorBoundaryHeuristic(const string& bnd_heur_str,
					  vector<BoundaryHeuristic>& bnd_heur_v)
{
  if (bnd_heur_str.size() == 0) {
    // default case.
    // first by weight
    bnd_heur_v.push_back(IH_MIN_WEIGHT); 
    // then by fill in if weight in tie
    bnd_heur_v.push_back(IH_MIN_FILLIN); 
  } else {
    for (unsigned i=0;i<bnd_heur_str.size();i++) {
      switch (bnd_heur_str[i]) {
      case 'S':
	bnd_heur_v.push_back(IH_MIN_SIZE);
	break;
      case 'F':
	bnd_heur_v.push_back(IH_MIN_FILLIN);
	break;
      case 'W':
	bnd_heur_v.push_back(IH_MIN_WEIGHT);
	break;
      case 'E':
	bnd_heur_v.push_back(IH_MIN_ENTROPY);
	break;
      case 'C':
	bnd_heur_v.push_back(IH_MIN_MAX_C_CLIQUE);
	break;
      case 'M':
	bnd_heur_v.push_back(IH_MIN_MAX_CLIQUE);
	break;
      case 'A':
	bnd_heur_v.push_back(IH_MIN_STATE_SPACE);
	break;
      case 'Q':
	bnd_heur_v.push_back(IH_MIN_C_STATE_SPACE);
	break;
      case 'N':
	bnd_heur_v.push_back(IH_MIN_WEIGHT_NO_D);
	break;

      case 'p':
	bnd_heur_v.push_back(IH_MIN_MIN_POSITION_IN_FILE);
	break;
      case 'P':
	bnd_heur_v.push_back(IH_MIN_MAX_POSITION_IN_FILE);
	break;

      case 't':
	bnd_heur_v.push_back(IH_MIN_MIN_TIMEFRAME);
	break;
      case 'T':
	bnd_heur_v.push_back(IH_MIN_MAX_TIMEFRAME);
	break;

      case 'h':
	bnd_heur_v.push_back(IH_MIN_MIN_HINT);
	break;
      case 'H':
	bnd_heur_v.push_back(IH_MIN_MAX_HINT);
	break;

      default:
	error("ERROR: Unknown triangulation heuristic given '%c' in string '%s'\n",
	      bnd_heur_str[i],bnd_heur_str.c_str());
	break;
      }
    }
  }
  return;
}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::computeFillIn()
 *   Computes the number of edges that would need to be added
 *   among 'nodes' to make 'nodes' complete.
 *
 * Preconditions:
 *   Set of nodes must be valid meaning that it has valid neighbors,
 *   parents, and children member variables.
 *
 * Postconditions:
 *   computed weight is provided.
 *
 * Side Effects:
 *     none
 *
 * Results:
 *     none
 *
 *
 *-----------------------------------------------------------------------
 */
int
BoundaryTriangulate::
computeFillIn(const set<RV*>& nodes) 
{

  int fill_in = 0;
  for (set<RV*>::iterator j=nodes.begin();
       j != nodes.end();
       j++) {

    // TODO: figure out if there is a way to just to compute
    // the size of the set intersection rather than to
    // actually produce the set intersection and then use its size.
    // Done - RR 7/11/12
    count_iterator<set <RV*> > tmp;
    set_intersection(nodes.begin(),nodes.end(),
		     (*j)->neighbors.begin(),(*j)->neighbors.end(),
		     tmp);

    // Nodes i and j should share the same neighbors except for
    // node i has j as a neighbor and node j has i as a
    // neighbor.  Therefore, if fill in is zero, then 
    //    tmp.size() == (*i)->neighbors.size() - 1
    // (we subtract 1 since we don't include the neighbor
    //  in the fill in, it is included in neighbors 
    //  but not tmp).
    // In otherwords, we have:

    fill_in += (nodes.size() - 1 - tmp.count());
  }
  // counted each edge twice, so fix that (although not 
  // strictly necessary since we could just compute with 2*fill_in,
  // we do this, however, since the user will be happier. 
  fill_in /= 2;
  return fill_in;

}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::computeWeightedFillIn()
 *
 *   Computes the sum of the product of the cardinalities of the
 *   fill-in edges that would need to be added among 'nodes' to make
 *   'nodes' complete. In other words, we sum the we "weight" of
 *   each fill-in edge, and each fill-in edge's weight is the 
 *   product of the cardinalities of the edges two nodes. 
 *
 * Preconditions:
 *   Set of nodes must be valid meaning that it has valid neighbors,
 *   parents, and children member variables.
 *
 * Postconditions:
 *   computed weight is provided.
 *
 * Side Effects:
 *     none
 *
 * Results:
 *     The log base 10 weight of the weighted fill in.
 *
 *
 *-----------------------------------------------------------------------
 */
double
BoundaryTriangulate::
computeWeightedFillIn(const set<RV*>& nodes) 
{


  // start off with some very small value, which
  // survives if there is no fill-in (meaning this
  // case would be chosen).
  double weight = -1e10;
  bool start = true;
  for (set<RV*>::iterator i=nodes.begin();
       i != nodes.end();
       i++) {
    for (set<RV*>::iterator j=nodes.begin();
	 j != nodes.end();
	 j++) {
      
      if (i != j) {
	if ((*j)->neighbors.find((*i)) == (*j)->neighbors.end()) {
	  // then we would need to fill it in.
	  double crrnt_weight;
	  set<RV*> edge;
	  edge.insert((*i));
	  crrnt_weight = MaxClique::computeWeight(edge,(*j));
	  if (start) {
	    start = false;
	    weight = crrnt_weight; 
	  } else {
	    weight = log10add(crrnt_weight,weight);
	  } 
	}
      }
    }
  }

  // Note, we counted each edge twice, so fix that, although not strictly
  // necessary since we could just compute with 2*fill_in_weight, we do this,
  // however, since the user will be happier.
  weight -= log10(2.0);
  return weight;
}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::graphWeight()
 *   Calculate the weight of a graph from a vector of cliques.  
 *
 * Preconditions:
 *   none
 * 
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   The log base 10 weight of the graph
 *-----------------------------------------------------------------------
 */
double 
BoundaryTriangulate::
graphWeight(vector<MaxClique>& cliques)
{
  vector<MaxClique>::iterator crrnt_clique; 
  vector<MaxClique>::iterator end_clique; 
  double crrnt_weight;
  double weight = -1;

  for( end_clique=cliques.end(), crrnt_clique=cliques.begin(); 
       crrnt_clique != end_clique;
       ++crrnt_clique )
  {
    crrnt_weight = MaxClique::computeWeight(crrnt_clique->nodes);

    if (weight < 0) {
      weight = crrnt_weight; 
    }
    else {
      weight = log10add(crrnt_weight,weight);
    } 
  } 

  return(weight);
}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::graphWeight()
 *   Calculate the weight of a graph from a vector of cliques.  This
 *   version also allows for an estimate of the JT weight (i.e., how
 *   much this set of cliques would cost when set up in a junction
 *   tree and used during the collect evidence stage in inference).
 *
 * Preconditions:
 *   none
 * 
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   Changes some of the JT members of maxcliques.
 *
 * Results:
 *   The log base 10 weight of the graph
 *-----------------------------------------------------------------------
 */
double 
BoundaryTriangulate::
graphWeight(vector<MaxClique>& cliques,		     
	    const bool useJTWeight,
	    const set<RV*>& interfaceNodes)
{
  if (!useJTWeight)
    return graphWeight(cliques);
  else
    return JunctionTree::junctionTreeWeight(cliques,interfaceNodes,lp_nodes,rp_nodes);
}



/*******************************************************************************
 *******************************************************************************
 *******************************************************************************
 **
 **         Main Partition Routines
 **
 *******************************************************************************
 *******************************************************************************
 *******************************************************************************
 */


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::findPartitions()
 *  Create the three partitions (P,C,E) of the template using
 *  the heuristics supplied.
 *     fh = a string with triangulation heuristics to use (in order)
 *     flr = force the use of either left or right, rather than use min.
 *     findBestBoundary = T/F if to use the exponential boundary finding alg.
 *
 * Preconditions:
 *   Object must be instantiated and have the use of the information
 *   in a valid FileParser object (which stores the parsed structure file)
 *   Arguments must indicate valid heuristics to use.
 *
 * Postconditions:
 *   Arguments Pc, Cc, and Ec now contain partitions in a separate
 *   graph from the FileParser (i.e., file parser information is
 *   not disturbed)
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   Pc, Cc, and Ec
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::findPartitions(// boundary quality heuristic
		 const string& bnd_heur_str,  
		 // force use of only left or right interface
		 const string& flr, 
		 // triangualtion heuristic to use
		 const string& tri_heur_str, 
		 // should we run the exponential find best interface
		 const bool findBestBoundary,
		 // the resulting template
		 GMTemplate& gm_template)
{
  //
  // M = number of chunks in which boundary algorithm is allowed to
  // exist i.e., a boundary is searched for within M repeated chunks,
  // where M >= 1.  Note that M and S put constraints on the number of
  // time frames of the observations (i.e., the utterance length in
  // time frames).  Specificaly, if N = number of frames of utterance,
  // then we must have N = length(P) + length(E) + (M+k*S)*length(C)
  // where k = 0,1,2,3,4, ... is some integer >= 0.  Therefore, making M
  // and/or S larger reduces the number valid possible utterance
  // lengths.

  infoMsg(High,"Finding Partitions under M=%d,S=%d with template of [P=%d,C=%d,E=%d] %d total frames\n",M,S,fp.numFramesInP(),fp.numFramesInC(),fp.numFramesInE(),fp.numFrames());

  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // create a network (called u2, but could be called "find
  // boundary" or "boundary search") that is used to find the boundary
  // in. This network is the template unrolled M+1 times. M = Max
  // number simultaneous chunks in which interface boundary may
  // exist. By unrolling M+1 times, we create a network with M+2
  // chunks. The first and last one are used to ensure boundary
  // algorithm works (since we are not guaranteed that P or E will
  // exist), and the middle M chunks are used to find the boundary. In
  // other words, we get:
  // 
  //     P C [ C C ... C ] C E
  //           1 2 ... M    
  //
  // where we find the boundary in the chunks within the square brackets. The
  // set of nodes consitituting the boundary is not allowed to exist in more than M chunks.
  // The various chunks will be placed into variables (with u2 suffix) P, C1, C2, C3, and E
  // as follows:
  //
  //
  //           1 2 ... M    
  //     P C [ C C ... C ] C E
  //    /  |    \     /    |  \       connections
  //   P  C1      C2       C3  E      <== u2 suffixed variables.
  // 

  vector <RV*> unroll2_rvs;
  map < RVInfo::rvParent, unsigned > unroll2_pos;
  fp.unroll(M+1,unroll2_rvs,unroll2_pos);
  // drop all the edge directions
  for (unsigned i=0;i<unroll2_rvs.size();i++) {
    unroll2_rvs[i]->createNeighborsFromParentsChildren();
  }
  // add any local cliques from .str file
  fp.addUndirectedFactorEdges(M+1,unroll2_rvs,unroll2_pos);
  // moralize graph
  for (unsigned i=0;i<unroll2_rvs.size();i++) {
    unroll2_rvs[i]->moralize();
  }
  // create sets P, C1, C2, C3, and E, from graph unrolled M+1 times
  // prologue
  set<RV*> P_u2;
  // 1st chunk, 1 chunk long
  set<RV*> C1_u2;
  // 2nd chunk, M chunks long
  set<RV*> C2_u2;
  // 3rd chunk, 1 chunk long
  set<RV*> C3_u2;
  // epilogue
  set<RV*> E_u2;
  int start_index_of_C1_u2 = -1;
  int start_index_of_C2_u2 = -1;
  int first_frame_of_C2_u2 = -1;
  int start_index_of_C3_u2 = -1;
  int start_index_of_E_u2 = -1;
  for (unsigned i=0;i<unroll2_rvs.size();i++) {
    if (unroll2_rvs[i]->frame() < fp.firstChunkFrame())
      // prologue
      P_u2.insert(unroll2_rvs[i]);
    else if (unroll2_rvs[i]->frame() <= fp.lastChunkFrame()) {
      // 1st chunk, 1 chunk long
      C1_u2.insert(unroll2_rvs[i]);
      if (start_index_of_C1_u2 == -1)
	start_index_of_C1_u2 = i;
    } else if (unroll2_rvs[i]->frame() <= fp.lastChunkFrame()+M*fp.numFramesInC()) {
      // 2nd chunk, M chunks long
      C2_u2.insert(unroll2_rvs[i]);
      if (start_index_of_C2_u2 == -1)
	start_index_of_C2_u2 = i;
      if (first_frame_of_C2_u2 == -1)
	first_frame_of_C2_u2 = unroll2_rvs[i]->frame();
    } else if (unroll2_rvs[i]->frame() <= fp.lastChunkFrame()+(M+1)*fp.numFramesInC()) {
      // 3rd chunk, 1 chunk long
      C3_u2.insert(unroll2_rvs[i]);
      if (start_index_of_C3_u2 == -1)
	start_index_of_C3_u2 = i;
    } else {
      // epilogue
      E_u2.insert(unroll2_rvs[i]);
      if (start_index_of_E_u2 == -1)
	start_index_of_E_u2 = i;
    }
  }

  assert (M*C1_u2.size() == C2_u2.size());
  assert (C2_u2.size() == M*C3_u2.size());
  infoMsg(High,"Size of (P,C1,C2,C3,E) = (%d,%d,%d,%d,%d)\n",
	  P_u2.size(),C1_u2.size(),C2_u2.size(),C3_u2.size(),E_u2.size());


  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // Next, create the network from which the new P', C', and E'
  // partitions will be formed out of the original P,C, and E in the
  // structure file. P', C', E' are formed based on both the current
  // settings of M and S, and also based on the resulting boundary
  // that is used. I.e., this new P', C', and E' will be the graph
  // partitions that are ultimately triangulated, where the new C'
  // will be unrolled as appropriate to get a full network. We call
  // the network from which P', C', and E' are created u1 (but could
  // call it "partition"). We place the new network in variables P_u1,
  // C1_u1, Cextra_u1, C2_u1, and E_u1 from which the final true
  // template is formed. Note that some of these might have variable
  // overlap (depending on M and S, and depending on the underlying
  // order with respect to C of the DBN Markov property, where "order"
  // in this context means the number of C's spanned across by links
  // coming from C).
  //
  // We create this graph by unrolling the basic template (M+S-1)
  // times (see below for more details). Here are some examples of
  // creating P_u1, C1_u1, Cextra_u1, C2_u1, and E_u1 for various
  // M and S values:
  // 
  //   M=1,S=1:  
  //       unrolled: P  C   C   E
  //                 |  |   |   |
  //       named:    P  C1  C2  E
  //
  //   M=1,S=2:  
  //       unrolled: P  C   C    C   E
  //                 |  |   |    |   |
  //       named:    P  C1 Cxtr  C2  E
  //
  //   M=2,S=1:  
  //       unrolled: P  C   C   C  E
  //                 |   \ / \ /   |
  //       named:    P    C1  C2   E
  //    (note that here C1 and C2 overlap in the 2nd chunk, i.e.,
  //     intersect(C1,C2) = 2nd C
  //
  //   M=2,S=2:
  //       unrolled: P  C   C   C  C   E
  //                 |   \ /     \ /   |
  //       named:    P    C1      C2   E
  //   (here, since M=S, there is no C1 C2 overlap).
  //
  //   M=3,S=2:
  //       unrolled: P  C   C   C  C  C   E
  //                 |   \ /   / \  \/    |
  //                 |    |  /    \ /     | 
  //       named:    P    C1       C2     E
  //   (here, since M>S, there is again a C1 C2 overlap of (M-S) chunks).
  //
  //   M=3,S=1:
  //       unrolled: P  C   C   C   C  E
  //                 |   \ / \ / \ /   |
  //                 |    |  / \  /    | 
  //       named:    P    C1    C2     E
  //   (here, since M>S, there is again a C1 C2 overlap of (M-S) chunks).
  //   In other words, intersect(C1,C2) = 2nd and 3rd C.
  // 
  //   M=2,S=3:
  //          P  C  C   C   C  C   E
  //          |   \ /   |    \ /   |
  //          P    C1   Cxtr  C2   E
  //    (here, again there is no intersection between C1 and C2, but
  //     since S>M, we put the middle in Cextra.
  // 
  // The number of chunks needed is M + S, so we should unroll (M+S-1)
  // time(s).  We ultimately create sets P', C1', C2', and E', from the
  // above graph unrolled (S+M-1) time(s), where each hyper-chunk C1'
  // and C2' are M frames long, and the difference in start frames
  // between chunk C1' and C2' is S frames.
  // Note also that:
  //      1) if S==M, Cextra will be empty, and C1' and C2' will not overlap.  
  //      2) if M > S, then C1' and C2' will overlap certain variables, and Cextra
  //         will be empty.  
  //      3) if S > M, C1' and C2' will not overlap, and there will be chunks 
  //         between C1' and C2', and they'll be placed in Cextra.
  // 
  // Examples. Note that ALL ARE IN THE LEFT INTERFACE
  // INITIAL-BOUNDARY CASE (i.e., before the boundary algorithm is
  // run). To get the right-interface case, just swap the E's with the
  // P's, and do a temporal inversion. In each case, the boundary is
  // constrained to lie entirely within M chunks, and when we unroll,
  // we skip by S chunks.  Examples (corresponding to the cases
  // above):
  // 
  //   M=1,S=1:  
  //          P  C  C  E
  //          |  |   \/
  //          P' C'  E' 
  // 
  //   M=1,S=2:  
  //          P  C  C  C  E
  //          |  | /    \/
  //          P' C'     E' 
  //
  //   M=2,S=1:
  //          P  C  C C  E
  //          |  |   \|_/
  //          P' C'   E' 
  //
  //   M=2,S=2:
  //        P  C C C C  E
  //        |  |/   \|_/
  //        P' C'    E'  
  // 
  //   M=3,S=2:
  //      P  C  C  C  C  C E
  //      |  | /    \ |  / /
  //      |   |      \| /_/
  //      P'  C'      E'  
  //        
  // 
  //   M=3,S=1:
  //        P  C   C  C  C  E
  //        |  |    \ |  / /
  //        |  |     \| /_/ 
  //       P'  C'     E'
  // 
  //
  //   M=2,S=3:
  //          P  C C C C C  E
  //          |   \|/   \|_/
  //          P'   C'    E'  
  //
  // Note that in the basic interface (not running the boundary
  // algorithm):
  //      1) C' has S C-chunks, 
  //      2) E' contains an E and an extra M C-chunks. 
  // The reason for the extra M C-chunks in E' is that the
  // boundary may span M chunks into E'.
  // 
  // Note also that it is only in the M=1,S=1 case above can we
  // recover the basic template P,C,E by using P'E' (i.e., not using
  // C'). Therefore it is crucial that the following three have the same interfaces
  // (again in the left interface case):
  //     1) between P' and C'E'
  //     2) between P'C' and E'
  //     3) between P' and E'
  // This is the only graphical restriction paced on the template
  // (other than the obvious no directed cycles), so that we can use
  // the case of unrolling by zero (i.e., by not using C'). 
  // 
  // For a static network, one should use M=S=1, and make P empty, and
  // use a quick dummy triangulation for C', and spending all the time
  // triangulating E'.
  // 
  // Note that P or E (but not both) can be empty. When P is empty,
  // there are no interface restrictions (since there is only one
  // possible interface, between C' and E'). 
  //

  vector <RV*> unroll1_rvs;
  map < RVInfo::rvParent, unsigned > unroll1_pos;
  fp.unroll(M+S-1,unroll1_rvs,unroll1_pos);
  for (unsigned i=0;i<unroll1_rvs.size();i++) {
    unroll1_rvs[i]->createNeighborsFromParentsChildren();
  }
  // Add edges from any extra factors in .str file. Note
  // we add them here so that the resulting clique struture has
  // these edges, but as of 1/2009 the edges from these cliques
  // are not added vai the unroll process (they need not be as
  // long as the inference code respects the resulting JT implied
  // by the given triangulation)
  fp.addUndirectedFactorEdges(M+S-1,unroll1_rvs,unroll1_pos);
  for (unsigned i=0;i<unroll1_rvs.size();i++) {
    unroll1_rvs[i]->moralize();
  }
  set<RV*> P_u1;
  set<RV*> C1_u1;
  set<RV*> Cextra_u1;
  set<RV*> C2_u1;
  set<RV*> E_u1;
  int start_index_of_C1_u1 = -1;
  int start_index_of_C2_u1 = -1;
  int start_index_of_E_u1 = -1;
  int first_frame_of_C1_u1 = -1;
  int first_frame_of_C2_u1 = -1;
  for (unsigned i=0;i<unroll1_rvs.size();i++) {
    // Note that there are some casts to (int) in the below below
    // since it might be the case that M = 0, and if unsigned
    // comparisions are used, the condition could fail inappropriately
    if (unroll1_rvs[i]->frame() < fp.firstChunkFrame())
      P_u1.insert(unroll1_rvs[i]);
    if ((unroll1_rvs[i]->frame() >= fp.firstChunkFrame()) &&
	((int)unroll1_rvs[i]->frame() <= (int)fp.lastChunkFrame() + ((int)M-1)*(int)fp.numFramesInC())) {
      C1_u1.insert(unroll1_rvs[i]);
      if (start_index_of_C1_u1 == -1)
	start_index_of_C1_u1 = i;
      if (first_frame_of_C1_u1 == -1)
	first_frame_of_C1_u1 = unroll1_rvs[i]->frame();
    }
    if (((int)unroll1_rvs[i]->frame() > (int)fp.lastChunkFrame() + ((int)M-1)*(int)fp.numFramesInC()) &&
	(unroll1_rvs[i]->frame() < fp.firstChunkFrame() + S*fp.numFramesInC())) {
      // this should only happen when S > M
      assert ( S > M );
      Cextra_u1.insert(unroll1_rvs[i]);
    }
    if ((unroll1_rvs[i]->frame() >= fp.firstChunkFrame() + S*fp.numFramesInC()) &&
	((int)unroll1_rvs[i]->frame() <= (int)fp.lastChunkFrame() + ((int)M+(int)S-1)*(int)fp.numFramesInC())) {
      C2_u1.insert(unroll1_rvs[i]);
      if (start_index_of_C2_u1 == -1)
	start_index_of_C2_u1 = i;
      if (first_frame_of_C2_u1 == -1)
	first_frame_of_C2_u1 = unroll1_rvs[i]->frame();
    }
    if ((int)unroll1_rvs[i]->frame() > (int)fp.lastChunkFrame() + ((int)M+(int)S-1)*(int)fp.numFramesInC()) {
      E_u1.insert(unroll1_rvs[i]);
      if (start_index_of_E_u1 == -1)
	start_index_of_E_u1 = i;
    }
  }

  // sanity checks
  assert (C1_u1.size() == C2_u1.size());
  assert (C1_u1.size() == C2_u2.size());
  assert ((S<=M) || (Cextra_u1.size() == C1_u2.size()*(S-M)));
  infoMsg(High,"Size of (P,C1,Cextra,C2,E) = (%d,%d,%d,%d,%d)\n",
	  P_u1.size(),C1_u1.size(),Cextra_u1.size(),C2_u1.size(),E_u1.size());




  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // Next, create a network unrolled 0 times in the modified partition
  // (corresponding exactly to the basic user defined M/S-modified
  // template). This is used only for interface error checks, for 
  // E' to P' matching the other cases (see below).
  // 
  vector <RV*> unroll0_rvs;
  map < RVInfo::rvParent, unsigned > unroll0_pos;
  fp.unroll(M-1,unroll0_rvs,unroll0_pos);
  // drop all the edge directions
  for (unsigned i=0;i<unroll0_rvs.size();i++) {
    unroll0_rvs[i]->createNeighborsFromParentsChildren();
  }
  // add edges from any extra factors in .str file
  fp.addUndirectedFactorEdges(M-1,unroll0_rvs,unroll0_pos);
  // moralize graph
  for (unsigned i=0;i<unroll0_rvs.size();i++) {
    unroll0_rvs[i]->moralize();
  }
  // create sets P0, C0, E0, from graph unrolled 0 times
  // In left interface case, P' = P0, C = empty, and E' = [C0 E0]
  // In right interface case, P' = [P0 C0], C = empty, and E' = E0
  set<RV*> P0;
  set<RV*> C0;
  set<RV*> E0;

  for (unsigned i=0;i<unroll0_rvs.size();i++) {
    if (unroll0_rvs[i]->frame() < fp.firstChunkFrame()) {
      // prologue
      P0.insert(unroll0_rvs[i]);
    } else if (unroll0_rvs[i]->frame() <= fp.lastChunkFrame()+(M-1)*fp.numFramesInC()) {
      // middle section, M chunks long.
      C0.insert(unroll0_rvs[i]);
    } else {
      // epilogue
      E0.insert(unroll0_rvs[i]);
    }
  }
  infoMsg(High,"Size of (P0,C0,E0) = (%d,%d,%d)\n",
	  P0.size(),C0.size(),E0.size());


  // augmentToAbideBySMarkov(unroll0_rvs,unroll0_pos,augmentation2,C0,fp.firstChunkFrame());



  // figure out which interfaces to compute.
  bool do_left_interface=false;
  bool do_right_interface=false;  
  if (flr.size() == 0 || (flr.size() > 0 && toupper(flr[0]) != 'R')) {
    // then user asked to do left interface.
    if (!validInterfaceDefinition(P_u1,C1_u1,Cextra_u1,C2_u1,E_u1,S,unroll1_rvs,unroll1_pos,
				  P0,C0,E0,unroll0_rvs,unroll0_pos,true))
      infoMsg(Warning,"WARNING: Can't compute left interface since structure template not compatible with this case.\n");
    else
      do_left_interface=true;
  }
  if (flr.size() == 0 || (flr.size() > 0 && toupper(flr[0]) != 'L')) {
    // then user asked to do right interface.
    if (!validInterfaceDefinition(E_u1,C2_u1,Cextra_u1,C1_u1,P_u1,S,unroll1_rvs,unroll1_pos,
				  E0,C0,P0,unroll0_rvs,unroll0_pos,false))
      infoMsg(Warning,"WARNING: Can't compute right interface since structure template not compatible with this case.\n");
    else
      do_right_interface=true;
  }
  if (do_left_interface == false && do_right_interface == false) {
    // then neither left nor right interface approach is valid. This
    // results when we have a template such that neither the interface
    // between P and C, nor the interface between C and E is
    // compatible with the interface between C and C. Since we run the
    // boundary algorithm only one time (for interface between C and
    // C), we don't deal with this situation (i.e., it is a constraint
    // on the .str file), so we die with an error here.
    error("ERROR: Can not successfully compute either a left or a right interface.\n"
	  "Either structure file is not valid for both left & right interface or\n"
          "under current force L/R command-line options.  Check if P contains extra\n"
	  "backwards-time links from C to P (that don't exist between neighboring\n"
          "Cs) and/or if E contains extra forwards-time links from C to E (that\n"
          "don't exist between neighboring Cs) In left interface case, interface\n"
          "edges must be compatible between 3 following cases ('|' denotes\n"
          "interface cut between left and right half in each case):\n"
	  "   P | C(1) C(2) ... C(M) E\n"
	  "   P | C(1)  C(2)   ...     C(M+S) E\n"
	  "   P  C(1) ...  C(S) | C(S+1) ... C(M+S) E\n"
	  "In right interface case, interface edges must be compatible between 3\n"
          "following cases:\n"
	  "   P C(1) C(2) ... C(M) | E\n"
	  "   P C(1)  C(2)   ...     C(M+S) | E\n"
	  "   P C(1) ...  C(S) | C(S+1) ... C(M+S) E\n"
	  "Currently, M=%d,S=%d.",M,S);
  }

  // TODO: optionally call addExtraEdgesToGraph(nodes, edge_heuristic)
  // to add edges corresponding to ancestral pairs. 
  // TODO: change name of routine 'addExtraEdgesToGraph' to something like
  //     connectAncestralPairs(), or addEdgesForAncestralPairs(), etc.
  // 


  // now, augment the network to partition.
  set < RVInfo::rvParent > augmentation2;
  computeSMarkovAugmentation(unroll1_rvs,unroll1_pos,
			     P_u1,
			     C1_u1,first_frame_of_C1_u1,
			     Cextra_u1,
			     C2_u1,first_frame_of_C2_u1,
			     E_u1,
			     augmentation2);

  // printf("C1_u1:");printRVSet(stdout,C1_u1);
  // printf("C2_u1:");printRVSet(stdout,C2_u1);
  assert ( C1_u1.size() == C2_u1.size() );

  if (message(Max)) {
    printf("Relative C*S augmentation2 is: ");
    set < RVInfo::rvParent >::iterator it;
    for (it = augmentation2.begin();it != augmentation2.end(); it ++) {
      RVInfo::rvParent p = (*it);
      printf("%s(%d),",p.first.c_str(),p.second);
    }
    printf("\n");
  }

  augmentToAbideBySMarkov(unroll2_rvs,unroll2_pos,augmentation2,C2_u2,first_frame_of_C2_u2);


  // printf("C2_u2:");printRVSet(stdout,C2_u2);
  assert (C2_u2.size() == C1_u1.size());

  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////

  // create mapping from C2_u2 (for which we now
  // have an appropriate interface vars) to nodes C1_u1 and C2_u1
  // which we are going to use to set up the 3-way partition
  // to ultimately triangulate.
  map < RV*, RV* > C2_u2_to_C1_u1;
  map < RV*, RV* > C2_u2_to_C2_u1;
  //   for (unsigned i=0;i<C2_u2.size();i++) {
  //     C2_u2_to_C1_u1[unroll2_rvs[i+start_index_of_C2_u2]]
  //       = unroll1_rvs[i+start_index_of_C1_u1];
  //     C2_u2_to_C2_u1[unroll2_rvs[i+start_index_of_C2_u2]]
  //       = unroll1_rvs[i+start_index_of_C2_u1];
  //   }
  set<RV*>::iterator c2_u2_iter;
  for (c2_u2_iter = C2_u2.begin(); c2_u2_iter != C2_u2.end(); c2_u2_iter ++) {  
    RV* rv = (*c2_u2_iter);
    C2_u2_to_C1_u1[rv] = getRV(unroll1_rvs,unroll1_pos,(rv),-fp.numFramesInC());
    C2_u2_to_C2_u1[rv] = getRV(unroll1_rvs,unroll1_pos,(rv),(S-1)*fp.numFramesInC());
  }


  // allocate space for results
  // left of the left interface in u2C2
  set<RV*> left_C_l_u2C2;
  // the left interface in u2C2
  set<RV*> C_l_u2C2;
  // right of the right interface in u2C2
  set<RV*> right_C_r_u2C2;
  // the right interface in u2C2
  set<RV*> C_r_u2C2;

  vector<BoundaryHeuristic> bnd_heur_v;
  createVectorBoundaryHeuristic(bnd_heur_str,bnd_heur_v);

  TriangulateHeuristics tri_heur;
  parseTriHeuristicString(tri_heur_str,tri_heur);

  vector<float> best_L_score;
  vector<float> best_R_score;

  if (do_left_interface) {
    infoMsg(Tiny,"---\nFinding BEST LEFT interface\n");
    // find best left interface
    findingLeftInterface = true;
    

    set<RV*> C2_1_u2; // first chunk in C2_u2
    if (M == 1) {
      // make it empty signaling that we don't bother to check it in this case.
      C2_1_u2.clear();
    } else {
      for (set<RV*>::iterator i=C2_u2.begin();
	   i != C2_u2.end();i++) {
	if ((*i)->frame() > fp.lastChunkFrame() && (*i)->frame() <= fp.lastChunkFrame()+fp.numFramesInC())
	  C2_1_u2.insert((*i));
      }
    }
    findBestInterface(P_u2,C1_u2,C2_u2,C2_1_u2,C3_u2,E_u2,
		      left_C_l_u2C2,C_l_u2C2,best_L_score,
		      bnd_heur_v,
		      findBestBoundary,
		      // find best boundary args
		      tri_heur,
		      P_u1,
		      C1_u1,
		      Cextra_u1,
		      C2_u1,
		      E_u1,
		      C2_u2_to_C1_u1,
		      C2_u2_to_C2_u1
		      );
  }
  if (do_right_interface) {
    infoMsg(Tiny,"---\nFinding BEST RIGHT interface\n");
    // find best right interface
    findingLeftInterface = false;

    set<RV*> C2_l_u2; // last chunk of C2_u2
    if (M == 1) {
      // make it empty signaling that we don't bother to check it in this case.
      C2_l_u2.clear();
    } else {
      for (set<RV*>::iterator i=C2_u2.begin();
	   i != C2_u2.end();i++) {
	if ((*i)->frame() > (fp.lastChunkFrame()+(M-1)*fp.numFramesInC()) 
	    && (*i)->frame() <= (fp.lastChunkFrame()+M*fp.numFramesInC()))
	  C2_l_u2.insert((*i));
      }
    }
    findBestInterface(E_u2,C3_u2,C2_u2,C2_l_u2,C1_u2,P_u2,
		      right_C_r_u2C2,C_r_u2C2,best_R_score,
		      bnd_heur_v,
		      findBestBoundary,
		      // find best boundary args
		      tri_heur,
		      E_u1,
		      C2_u1,
		      Cextra_u1,
		      C1_u1,
		      P_u1,
		      C2_u2_to_C2_u1,
		      C2_u2_to_C1_u1
		      );
  }


  // Now find the partitions (i.e., left or right) corresponding
  // the interface which had minimum size, prefering the left
  // interface if there is a tie.
#if 0
  if ((best_L_score.size() > 0) &&
      ((best_R_score.size() == 0) // not valid structure file for right interface
       ||
       (flr.size() > 0 && toupper(flr[0]) == 'L') // we forced to the left and never computed right
       || 
       (flr.size() == 0 && best_L_score <= best_R_score) // both were scored and left is better or tied
       )
      ) {
#endif
  if ((!do_right_interface
       || 
       (do_left_interface
	&& (best_L_score <= best_R_score)))) {

    findingLeftInterface = true;
    infoMsg(Tiny,"---\nUsing left interface to define partitions\n");

    // this next routine gives us the best left interface that
    // exists from within the chunk C2_u2 and places
    // it in C_l_u2, and everything to the 'left' of C_l_u2
    // that still lies within C2_u2 is placed in left_C_l_u2

    constructInterfacePartitions(P_u1,
				 C1_u1,
				 Cextra_u1,
				 C2_u1,
				 E_u1,
				 C2_u2_to_C1_u1,
				 C2_u2_to_C2_u1,
				 left_C_l_u2C2,
				 C_l_u2C2,
				 gm_template);

    // Write information about how boundary was created to a string,
    // but make sure there is no white space in the string.
    char buff[2048];
    sprintf(buff,"Left_interface:Run_Bdry_Alg(%c),Bnd_Heurs(%s),TravFrac(%f),Tri_Heur(%s)",
	    (findBestBoundary? 'T' : 'F'),
	    bnd_heur_str.c_str(),
	    boundaryTraverseFraction,
	    tri_heur_str.c_str());
    gm_template.boundaryMethod = buff;
    gm_template.leftInterface = true; 
  } else {
    // find right interface partitions
    findingLeftInterface = false;
    infoMsg(Nano,"---\nUsing right interface to define partitions\n");


    // create a mapping from C1_u2 to C1_u1 and from C1_u2 to P_u1
    // (where a correspondence exists)

    constructInterfacePartitions(E_u1,
				 C2_u1,
				 Cextra_u1,
				 C1_u1,
				 P_u1,
				 C2_u2_to_C2_u1,
				 C2_u2_to_C1_u1,
				 right_C_r_u2C2,
				 C_r_u2C2,
				 gm_template);

    // Write information about how boundary was created to a string,
    // but make sure there is no white space in the string.
    char buff[2048];
    sprintf(buff,"Right_interface:Run_Bdry_Alg(%c),Bnd_Heurs(%s),TravFrac(%f),Tri_Heur(%s)",
	    (findBestBoundary? 'T' : 'F'),
	    bnd_heur_str.c_str(),
	    boundaryTraverseFraction,
	    tri_heur_str.c_str());
    gm_template.boundaryMethod = buff;
    gm_template.leftInterface = false; 

  }

  // TODO: delete everything.
  // for (unsigned i=0;i<unroll_s_markov_rvs.size();i++)
  // delete unroll_s_markov_rvs[i];

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::constructInterfacePartitions()
 *   Construct the three partitions, either left or right depending
 *   on the order of the arguments given.
 *
 * For the left interface, we create new P,C, and E variable sets where
 *  where P = modified prologue
 *  where C = modified chunk to repeat
 *  where E = modified epilogue to repeat
 *
 * Preconditions:
 *   The variable findingLeftInterface must be set to
 *   appropriate value before calling this routine.
 *
 * Postconditions:
 *
 * Side Effects:
 *   Makes the interfaces in C1_u1 and C2_u1 complete.
 *
 * Results:
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::constructInterfacePartitions(
 // input variables
 const set<RV*>& P_u1,
 const set<RV*>& C1_u1,
 const set<RV*>& Cextra_u1, // non-empty only when S > M
 const set<RV*>& C2_u1,
 const set<RV*>& E_u1,
 // these next 2 should be const, but there is no "op[] const"
 map < RV*, RV* >& C2_u2_to_C1_u1,
 map < RV*, RV* >& C2_u2_to_C2_u1,
 const set<RV*>& left_C_l_u2C2,
 const set<RV*>& C_l_u2C2,
 // output variables
 GMTemplate& gm_template)

{
  // now we need to make a bunch of sets to be unioned
  // together to get the partitions.
  set<RV*> C_l_u1C1;
  set<RV*> C_l_u1C2;

  set<RV*> left_C_l_u1C1;
  set<RV*> left_C_l_u1C2;

  assert (M > 0);

  for (set<RV*>::iterator i = C_l_u2C2.begin();
       i!= C_l_u2C2.end(); i++) {

    assert (C2_u2_to_C1_u1.find((*i)) != C2_u2_to_C1_u1.end());
    assert (C2_u2_to_C2_u1.find((*i)) != C2_u2_to_C2_u1.end());

    C_l_u1C1.insert(C2_u2_to_C1_u1[(*i)]);
    C_l_u1C2.insert(C2_u2_to_C2_u1[(*i)]);
  }

  for (set<RV*>::iterator i = left_C_l_u2C2.begin();
       i != left_C_l_u2C2.end(); i++) {
      
    assert (C2_u2_to_C1_u1.find((*i)) != C2_u2_to_C1_u1.end());
    assert (C2_u2_to_C2_u1.find((*i)) != C2_u2_to_C2_u1.end());
    
    left_C_l_u1C1.insert(C2_u2_to_C1_u1[(*i)]);
    left_C_l_u1C2.insert(C2_u2_to_C2_u1[(*i)]);
  }
  
  // Finally, create the modified sets P, C, and E
  //   where P = modified prologue
  //   where C = modified chunk to repeat
  //   where E = modified epilogue to repeat.
  // which are to be triangulated separately. 
  // For the *left* interface, the modified sets are defined
  // as follows:
  //
  //    1) P = P_u1 + C1_u1(left_C_l_u2) + C1_u1(C_l_u2)
  //    2) E = C2_u1\C2_u1(left_C_l_u2) + E_u1
  //    3) C = ((C1_u1 + C2_u1) \ (P + E)) + C1_u1(C_l_u2) + Cextra_u1 + C2_u1(C_l_u2) 
  //
  // and the symmetric definitions apply for the right interface.  We
  // use the left interface definitions in this code and assume the
  // caller calles with inverted arguments to get the right interface
  // behavior.  Note that the construction in line 3) uses the P, and E
  // constructed in line 1) and 2).

  // Finish P
  set<RV*> P = P_u1;
  set_union(left_C_l_u1C1.begin(),left_C_l_u1C1.end(),
	    C_l_u1C1.begin(),C_l_u1C1.end(),
	    inserter(P,P.end()));


  // Finish E
  set<RV*> E = E_u1;
  set_difference(C2_u1.begin(),C2_u1.end(),
		 left_C_l_u1C2.begin(),left_C_l_u1C2.end(),
		 inserter(E,E.end()));

  // Finish C
  set<RV*> C;
  set<RV*> tmp1;
  set<RV*> tmp2;
  set<RV*> tmp3;
  set_union(C1_u1.begin(),C1_u1.end(),
	    C2_u1.begin(),C2_u1.end(),
	    inserter(tmp1,tmp1.end()));
  // printf("tmp1=:");printRVSet(stdout,tmp1);
  set_union(P.begin(),P.end(),
	    E.begin(),E.end(),
	    inserter(tmp2,tmp2.end()));
  // printf("tmp2=:");printRVSet(stdout,tmp2);
  set_union(C_l_u1C2.begin(),C_l_u1C2.end(),
	    C_l_u1C1.begin(),C_l_u1C1.end(),
	    inserter(tmp3,tmp3.end()));
  // printf("tmp3=:");printRVSet(stdout,tmp3);
  set_union(Cextra_u1.begin(),Cextra_u1.end(),
	    tmp3.begin(),tmp3.end(),
	    inserter(C,C.end()));
  // printf("C=:");printRVSet(stdout,C);
  set_difference(tmp1.begin(),tmp1.end(),
		 tmp2.begin(),tmp2.end(),
		 inserter(C,C.end()));

  // TODO: if P (resp. E) is such that P_u1 (resp. E_u1) is empty, and
  //       also that left_C_l is empty, then should allow P to be empty
  //       rather than having non-useful completed interface only in P (since this
  //       will be in C as well. Same for E.

  // create the template with these partition definitions.

  if (findingLeftInterface)
    gm_template.createPartitions(P,C,E,C_l_u1C1,C_l_u1C2);
  else // we are finding right interface, need to swap arguments. 
    gm_template.createPartitions(E,C,P,C_l_u1C2,C_l_u1C1);


}


/*******************************************************************************
 *******************************************************************************
 *******************************************************************************
 **
 **         Main Triangulation Routines
 **
 *******************************************************************************
 *******************************************************************************
 *******************************************************************************
 */


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulate()
 *   Triangulate an entire GMTemplate, using the same method for each partition.
 *    
 * Preconditions:
 *   Graphs in GMTemplate must be a valid undirected model. This means if the graph
 *   was originally a directed model, it must have been properly
 *   moralized and their 'neighbors' structure is valid. It is also
 *   assumed that the parents of each r.v. are valid but that they
 *   only poiint to variables within the set 'nodes' (i.e., parents
 *   must not point out of this set).
 *
 *   Also, it is assumed that graphs are not yet triangulated. If they
 *   are, the result will still be triangulated, but it might add more
 *   edgeds to the already triangulated graph, depending on the
 *   triangulation heuristic given.
 *
 * Postconditions:
 *   Resulting graphs are  now triangulated, and cliques are stored (not in RIP order)
 *
 * Side Effects:
 *   Will change neighbors members of variables.
 *
 * Results:
 *     none
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::triangulate(const string& tri_heur_str,
	      bool jtWeight,
	      GMTemplate& gm_template,
	      bool doP,
	      bool doC,
	      bool doE)
{
  TriangulateHeuristics tri_heur;
  SavedGraph orgnl_P_nghbrs;
  SavedGraph orgnl_C_nghbrs;
  SavedGraph orgnl_E_nghbrs;
  string best_P_method_str;
  string best_C_method_str;
  string best_E_method_str;
  double best_P_weight = DBL_MAX;
  double best_C_weight = DBL_MAX;
  double best_E_weight = DBL_MAX;
  const set <RV*> emptySet;


  if (gm_template.P.nodes.size() == 0)
    doP = false;
  if (gm_template.C.nodes.size() == 0)
    doC = false;
  if (gm_template.E.nodes.size() == 0)
    doE = false;


  if (doP)
    saveCurrentNeighbors(gm_template.P,orgnl_P_nghbrs);
  if (doC)
    saveCurrentNeighbors(gm_template.C,orgnl_C_nghbrs);
  if (doE)
    saveCurrentNeighbors(gm_template.E,orgnl_E_nghbrs);

  parseTriHeuristicString(tri_heur_str,tri_heur);

  if (doP) {
    infoMsg(IM::Tiny, "---\nTriangulating P:\n");
    setUpForP(gm_template);
    triangulatePartition(gm_template.P.nodes,jtWeight,gm_template.PCInterface_in_P,tri_heur,orgnl_P_nghbrs,gm_template.P.cliques,gm_template.P.triMethod,best_P_weight);
  }
  if (doC) {
    infoMsg(IM::Tiny, "---\nTriangulating C:\n");
    setUpForC(gm_template);
    triangulatePartition(gm_template.C.nodes,jtWeight,gm_template.CEInterface_in_C,tri_heur,orgnl_C_nghbrs,gm_template.C.cliques,gm_template.C.triMethod,best_C_weight);
  }
  if (doE) {
    infoMsg(IM::Tiny, "---\nTriangulating E:\n");
    setUpForE(gm_template);
    triangulatePartition(gm_template.E.nodes,jtWeight,emptySet,tri_heur,orgnl_E_nghbrs,gm_template.E.cliques,gm_template.E.triMethod,best_E_weight);
  }

  ////////////////////////////////////////////////////////////////////////
  // Return with the best triangulations found, which is
  // be stored within the template at this point.
  ////////////////////////////////////////////////////////////////////////
  if (doP)
    restoreNeighbors(orgnl_P_nghbrs);
  if (doC)
    restoreNeighbors(orgnl_C_nghbrs);
  if (doE)
    restoreNeighbors(orgnl_E_nghbrs);

  gm_template.triangulatePartitionsByCliqueCompletion();
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulate()
 *   Triangualted GMTemplate using one-edge.  If the requested method is
 *   not one-edge, the main triangulation interface is called.  
 * 
 *   The separate interface for one-edge is needed because it returns a 
 *   modification of the input triangulation, where most other methods 
 *   either return an entirely new triangulation or keep the existing 
 *   triangulation intact.  
 *
 *  Preconditions:
 *   Graphs in GMTemplate must be a valid undirected model. This means if the
 *   graph was originally a directed model, it must have been properly
 *   moralized and their 'neighbors' structure is valid. It is also assumed
 *   that the parents of each r.v. are valid but that they only poiint to
 *   variables within the set 'nodes' (i.e., parents must not point out of
 *   this set).
 *
 * Postconditions:
 *   Resulting graphs are  now triangulated, and cliques are stored (not in
 *   RIP order)
 *
 * Side Effects:
 *   Will change neighbors members of variables.
 *
 * Results:
 *     none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::triangulate(const string& tri_heur_str,
	      bool jtWeight,
	      GMTemplate& gm_template,
              vector<MaxClique> orgnl_P_triangulation,
              vector<MaxClique> orgnl_C_triangulation,
              vector<MaxClique> orgnl_E_triangulation,
	      bool doP,
	      bool doC,
	      bool doE)
{
  SavedGraph orgnl_P_nghbrs;
  SavedGraph orgnl_C_nghbrs;
  SavedGraph orgnl_E_nghbrs;
  set<RV*>::iterator crrnt_RV;

  if (gm_template.P.nodes.size() == 0)
    doP = false;
  if (gm_template.C.nodes.size() == 0)
    doC = false;
  if (gm_template.E.nodes.size() == 0)
    doE = false;

  //////////////////////////////////////////////////////////////////////
  // If method is not one-edge, call antoher triangualte interface
  //////////////////////////////////////////////////////////////////////
  if (tri_heur_str != "one-edge") {
    triangulate( tri_heur_str, jtWeight, gm_template, doP, doC, doE ); 
  }
  //////////////////////////////////////////////////////////////////////
  // Triangualted requested partitions using one-edge 
  //////////////////////////////////////////////////////////////////////
  else {

    if (doP) {
      infoMsg(IM::Tiny, "---\nTriangulating P:\n");
      saveCurrentNeighbors(gm_template.P, orgnl_P_nghbrs);

      setUpForP(gm_template);

      if (gm_template.P.cliques.size() > 0) {
        for ( crrnt_RV = gm_template.P.nodes.begin();
              crrnt_RV != gm_template.P.nodes.end();
              ++crrnt_RV ) {
          (*crrnt_RV)->neighbors.clear();
        }
      }
      fillAccordingToCliques( orgnl_P_triangulation );
    
      triangulateOneEdgeChange(gm_template.P, orgnl_P_nghbrs );
      gm_template.P.triMethod = "one-edge";
    }
    if (doC) {
      infoMsg(IM::Tiny, "---\nTriangulating C:\n");
      saveCurrentNeighbors(gm_template.C, orgnl_C_nghbrs);
      setUpForC(gm_template);

      if (gm_template.C.cliques.size() > 0) {
        for ( crrnt_RV = gm_template.C.nodes.begin();
              crrnt_RV != gm_template.C.nodes.end();
              ++crrnt_RV ) {
          (*crrnt_RV)->neighbors.clear();
        }
      }
      fillAccordingToCliques( orgnl_C_triangulation );
 
      triangulateOneEdgeChange(gm_template.C, orgnl_C_nghbrs );
      gm_template.C.triMethod = "one-edge";
    }
    if (doE) {
      infoMsg(IM::Tiny, "---\nTriangulating E:\n");
    
      saveCurrentNeighbors(gm_template.E, orgnl_E_nghbrs);

      setUpForE(gm_template);

      if (gm_template.P.cliques.size() > 0) {
        for ( crrnt_RV = gm_template.E.nodes.begin();
              crrnt_RV != gm_template.E.nodes.end();
              ++crrnt_RV ) {
          (*crrnt_RV)->neighbors.clear();
        }
      }
      fillAccordingToCliques( orgnl_E_triangulation );

      triangulateOneEdgeChange(gm_template.E, orgnl_E_nghbrs );
      gm_template.E.triMethod = "one-edge";
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // Return with the best triangulations found, which is
  // be stored within the template at this point.
  ////////////////////////////////////////////////////////////////////////
  if (doP)
    restoreNeighbors(orgnl_P_nghbrs);
  if (doC)
    restoreNeighbors(orgnl_C_nghbrs);
  if (doE)
    restoreNeighbors(orgnl_E_nghbrs);
  gm_template.triangulatePartitionsByCliqueCompletion();

}

/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateOnce()
 *   Triangulates a graph once according to a TriangulateHeuristics struct
 *    
 * Preconditions:
 *   Graph must be a valid undirected model. This means if the graph
 *   was originally a directed model, it must have been properly
 *   moralized and their 'neighbors' structure is valid. It is also
 *   assumed that the parents of each r.v. are valid but that they
 *   only poiint to variables within the set 'nodes' (i.e., parents
 *   must not point out of this set).
 *
 * Postconditions:
 *   Resulting graph is now triangulated, and cliques are returned 
 *
 * Side Effects:
 *   Will change neighbors members of variables, even if the current
 *   triangulation does not beat best_weight.
 *
 * Results:
 *     none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateOnce(// input: nodes to be triangulated
	    const set<RV*>& nodes,
	    // use JT weight rather than sum of weight
	    const bool jtWeight,
	    // nodes that a JT root must contain (ok to be empty).
	    const set<RV*>& nodesRootMustContain,
	    // triangulation heuristic method
	    const TriangulateHeuristics& tri_heur,
	    // original neighbor structures
	    SavedGraph& orgnl_nghbrs,
	    // output: resulting max cliques
	    vector<MaxClique>& cliques,
	    // output: string giving resulting method used
	    string& meth_str)
{
  vector<RV*> order;
  string                  annealing_str;

  switch (tri_heur.style) {

    case TS_RANDOM:
      triangulateRandom( nodes, cliques );
      meth_str = "random";
      break;

    case TS_BASIC:
      basicTriangulate( nodes, tri_heur.heuristic_vector, tri_heur.numRandomTop, order, cliques);
      meth_str = tri_heur.basic_method_string;
      break;

    case TS_ANNEALING:
      triangulateSimulatedAnnealing(nodes, jtWeight, nodesRootMustContain,
				    cliques, 
				    order, 
                                    annealing_str );
      meth_str = "annealing-" + annealing_str;
      break;

    case TS_EXHAUSTIVE:
      triangulateExhaustiveSearch( nodes, jtWeight, nodesRootMustContain,
				   orgnl_nghbrs, cliques ); 
      meth_str = "exhaustive";
      break;

    case TS_MCS:
      triangulateMaximumCardinalitySearch(nodes, cliques, order );
      meth_str = "MCS";
      break;

    case TS_FRONTIER:
      triangulateFrontier(nodes, cliques);
      meth_str = "frontier";
      break;

    case TS_COMPLETED:
      triangulateCompletePartition( nodes, cliques );
      meth_str = "completed";
      break;

    default: 
      // shouldn't happen
      assert (0);
      break;
  }

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulate()
 *   Triangulate accoring to a tri_heur structure. 
 *    
 * Preconditions:
 *   Graph must be a valid undirected model. This means if the graph
 *   was originally a directed model, it must have been properly
 *   moralized and their 'neighbors' structure is valid. It is also
 *   assumed that the parents of each r.v. are valid but that they
 *   only poiint to variables within the set 'nodes' (i.e., parents
 *   must not point out of this set).
 *
 * Postconditions:
 *   -Graph is triangulated 
 *   -Maximal cliques are stored in best_cliques
 *   -String describing the method giving the best score is stored in 
 *    best_method
 *   -Best weight found is stored in best_weight
 *
 * Side Effects:
 *   Will change neighbors members of variables
 *
 * Results:
 *   none 
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::triangulatePartition(
  const set<RV*>& nodes,  // nodes to be triangulated
  const bool jtWeight,                // use JT weight rather than sum of weight
  const set<RV*>& nodesRootMustContain, // nodes that a JT root must
                                                    // contain (ok to be empty).
  const TriangulateHeuristics& tri_heur,    // triangulation method
  SavedGraph& orgnl_nghbrs,   // original neighbor structures
  vector<MaxClique>& best_cliques,       // output: resulting max cliques
  string& best_method,            // output: string giving resulting method used
  double& best_weight,            // weight to best
  string  best_method_prefix
  )
{
  vector<MaxClique>       cliques;
  vector<RV*>             order;
  TimerClass              type_timer;
#if 0
  // unused
  double                  prvs_best_weight;
#endif
  double                  weight;
  string                  meth_str;
  char                    buff[64];
  SavedGraph              nghbrs_with_extra;
  TriangulateHeuristics   ea_tri_heur;
  int                     trial;

  ////////////////////////////////////////////////////////////////////////
  // compute the real best weight for a set of current
  // cliques, if the weight has not already been computed.
  ////////////////////////////////////////////////////////////////////////
  if (best_weight == DBL_MAX && best_cliques.size() > 0) {
    best_weight = graphWeight(best_cliques,jtWeight,nodesRootMustContain);
  }

  ////////////////////////////////////////////////////////////////////////
  // Initialize variables 
  ////////////////////////////////////////////////////////////////////////
  //  prvs_best_weight = best_weight;
  ea_tri_heur = tri_heur;

  ////////////////////////////////////////////////////////////////////////
  // Add extra edges, if appropriate 
  ////////////////////////////////////////////////////////////////////////
  restoreNeighbors(orgnl_nghbrs);

  if ((tri_heur.extraEdgeHeuristic != NO_EDGES) && 
      (tri_heur.extraEdgeHeuristic != RANDOM_EDGES)) {
    addExtraEdgesToGraph( nodes, tri_heur.extraEdgeHeuristic );
  }

  saveCurrentNeighbors( nodes, nghbrs_with_extra ); 

  ////////////////////////////////////////////////////////////////////////
  // Repeat triangulation specified number of times 
  ////////////////////////////////////////////////////////////////////////
  trial = 0;
  type_timer.Reset( tri_heur.seconds );
  for (;
    (best_cliques.size()<=0) ||
    ( ((!timer) || (!timer->Expired())) && 
      (((tri_heur.numberTrials>0) && (trial<tri_heur.numberTrials)) ||
       ((tri_heur.seconds>0)      && (!type_timer.Expired()))) );
    trial++) {

    ////////////////////////////////////////////////////////////////////////
    // Initialize graph for new triangulation 
    ////////////////////////////////////////////////////////////////////////
    order.clear();
    cliques.clear();

    if (tri_heur.extraEdgeHeuristic == RANDOM_EDGES) {
      restoreNeighbors(orgnl_nghbrs);
      addExtraEdgesToGraph( nodes, tri_heur.extraEdgeHeuristic );
    }
    else {
      restoreNeighbors(nghbrs_with_extra);
    }

    switch (tri_heur.style) {

      ////////////////////////////////////////////////////////////////////////
      // Handle cases that keep their own running best weight 
      ////////////////////////////////////////////////////////////////////////
      case TS_COMPLETE_FRAME_LEFT:
        triangulateCompleteFrame( COMPLETE_FRAME_LEFT, nodes, jtWeight,
          nodesRootMustContain, tri_heur.basic_method_string,
          nghbrs_with_extra, best_cliques, best_method, best_weight,
          best_method_prefix+"completeframeleft" );
        break;

      case TS_COMPLETE_FRAME_RIGHT:
        triangulateCompleteFrame( COMPLETE_FRAME_RIGHT, nodes, jtWeight,
          nodesRootMustContain, tri_heur.basic_method_string,
          nghbrs_with_extra, best_cliques, best_method, best_weight,
          best_method_prefix+"completeframeright" );
        break;

      case TS_ELIMINATION_HEURISTICS:
        tryEliminationHeuristics( nodes, jtWeight, nodesRootMustContain, 
          nghbrs_with_extra, best_cliques, best_method, best_weight, 
          best_method_prefix );
        break;

      case TS_NON_ELIMINATION_HEURISTICS:
        tryNonEliminationHeuristics( nodes, jtWeight, nodesRootMustContain, 
          nghbrs_with_extra, best_cliques, best_method, best_weight);
        break;

      case TS_ALL_HEURISTICS:
        tryNonEliminationHeuristics( nodes, jtWeight, nodesRootMustContain, 
          nghbrs_with_extra, best_cliques, best_method, best_weight);
        tryEliminationHeuristics( nodes, jtWeight, nodesRootMustContain, 
          nghbrs_with_extra, best_cliques, best_method, best_weight, 
          best_method_prefix );
        break;

      case TS_PRE_EDGE_ALL:
        parseTriHeuristicString( tri_heur.basic_method_string, ea_tri_heur );
        ea_tri_heur.extraEdgeHeuristic = ALL_EDGES;
        sprintf(buff,"%d", trial+1 );
        triangulatePartition( nodes, jtWeight, nodesRootMustContain, ea_tri_heur, 
      			nghbrs_with_extra, best_cliques, best_method, best_weight, 
      			buff+(string)"-pre-edge-all-" );
        break;

      case TS_PRE_EDGE_LO:
        parseTriHeuristicString( tri_heur.basic_method_string, ea_tri_heur );
        ea_tri_heur.extraEdgeHeuristic = LOCALLY_OPTIMAL_EDGES;
        sprintf(buff,"%d", trial+1 );
        triangulatePartition( nodes, jtWeight, nodesRootMustContain, ea_tri_heur, 
      			nghbrs_with_extra, best_cliques, best_method, best_weight,
      			buff+(string)"-pre-edge-lo-" );
        break;

      case TS_PRE_EDGE_RANDOM:
        parseTriHeuristicString( tri_heur.basic_method_string, ea_tri_heur );
        ea_tri_heur.extraEdgeHeuristic = RANDOM_EDGES;
        sprintf(buff,"%d", trial+1 );
        triangulatePartition( nodes, jtWeight, nodesRootMustContain, ea_tri_heur, 
      			nghbrs_with_extra, best_cliques, best_method, best_weight,
      			buff+(string)"-pre-edge-random-" );
        break;

      case TS_PRE_EDGE_SOME:
        parseTriHeuristicString( tri_heur.basic_method_string, ea_tri_heur );
        ea_tri_heur.extraEdgeHeuristic = SOME_EDGES;
        sprintf(buff,"%d", trial+1 );
        triangulatePartition( nodes, jtWeight, nodesRootMustContain, ea_tri_heur, 
      			nghbrs_with_extra, best_cliques, best_method, best_weight,
      			buff+(string)"-pre-edge-some-" );
        break;

      ////////////////////////////////////////////////////////////////////////
      // Handle cases that triangulate once and do not keep a best weight 
      ////////////////////////////////////////////////////////////////////////
      default:
        triangulateOnce( nodes, jtWeight, nodesRootMustContain, tri_heur, 
          nghbrs_with_extra, cliques, meth_str);

        weight = graphWeight(cliques, jtWeight, nodesRootMustContain);

        if (weight < best_weight) 
        { 
          ////////////////////////////////////////////////////////////////////
          ////////////////////////////////////////////////////////////////////
          //@@@ There is a bug due to the stubbed out = operator in the 
          //   MaxClique 
          //   class the following line which clears the cliques appears to 
          //   work around the bug. @@@
          // TODO: ultimately take this out, but keep in for now until
          // we do code restructuring (1/20/2004).
          best_cliques.clear();
          ////////////////////////////////////////////////////////////////////
          ////////////////////////////////////////////////////////////////////

          best_cliques = cliques;
          best_weight  = weight;
  
          sprintf(buff,"%d-", trial+1 );
          best_method = best_method_prefix + buff + meth_str; 
          infoMsg(IM::Tiny, "   New Best Triangulation Found: %20s %-10f\n", 
            best_method.c_str(), weight);
        }
        break;
    }
  }

}

/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::basicTriangulate()
 *   The actual basic triangulation that does the work of
 *   elimination-based triangulation.
 *  
 *   This routine will triangulate a set of nodes using any
 *   combination of a number if different (but simple)
 *   elimination-triangulation heuristics such as (min weight, min
 *   size, min fill, etc.).  For a good description of these
 *   heuristics, see D. Rose et. al, 1970, 1976. The routine also
 *   allows for other heuristics to be used such as eliminate the
 *   earlier nodes (temporal order) first, or eliminate the nodes in
 *   order that they appear in the structure file (sometimes this
 *   simple constrained triangulation will work better than the
 *   "intelligent" heuristics, such as for certain lattice
 *   structures). The routine allows heuristics to be prioritized and
 *   combined, so that if there is a tie with the first heuristic, the
 *   second will be used, and if there is still a tie, the third one
 *   will be used, etc. Lastly, the routine allows random choices to
 *   be made over the best K cases at each elimination step.
 *     
 * Preconditions:
 *   Graph must be a valid undirected model and untriangulated. This means if the graph
 *   was originally a directed model, it must have been properly
 *   moralized and their 'neighbors' structure is valid. It is also
 *   assumed that the parents of each r.v. are valid but that they
 *   only poiint to variables within the set 'nodes' (i.e., parents
 *   must not point out of this set).
 *
 * Postconditions:
 *   Resulting graph is now triangulated.
 *
 * Side Effects:
 *   Might (and probably will unless graph is already triangulated and
 *   you get lucky by having found the perfect elimination order)
 *   change neighbors members of variables.
 *
 * Results:
 *     none
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::basicTriangulate(// input: nodes to triangulate
		 const set<RV*>& nodes,
		 // input: triangulation heuristic
		 const vector<BasicTriangulateHeuristic>& th_v,
		 // choose randomly from N-best scored nodes
		 const unsigned numRandomTop,
		 // output: nodes ordered according to resulting elimination
		 vector<RV*>& orderedNodes,  
		 // output: resulting max cliques
		 vector<MaxClique>& cliques,
		 // input: find the cliques as well
		 const bool findCliques
		 )
{
  const unsigned num_nodes = nodes.size();

  infoMsg(Huge,"\nBEGINNING BASIC TRIANGULATION --- \n");

  // need to use some form of heuristic
  assert ( th_v.size() != 0 );

  cliques.clear();

  // orderedNodes == already eliminated nodes. 
  orderedNodes.clear();

  // Also keep ordered (eliminated) nodes as a set for easy
  // intersection, with other node sets.
  set<RV*> orderedNodesSet;

  
  // Approach: essentially, create a priority queue data structure
  // using a multimap. In this case, those nodes with the lowest 'X'
  // where 'X' is the combined prioritized weight heuristics can be
  // accessed immediately in the front of the map. Also, whenever a
  // node gets eliminated, *ONLY* its neighbor's weights are
  // recalculated. With this data structure it is possible and
  // efficient to do so. This is because a multimap (used to simulate
  // a priority queue) has the ability to efficiently remove stuff
  // from the middle.
  multimap< vector<float> ,RV*> unorderedNodes;

  // We also need to keep a map to be able to remove elements of
  // 'unorderedNodes' when a node is eliminated. I.e., we need to be
  // able to map back from a RV* directly to its entry in the priority
  // queue so that when a node is eliminated, its neighbors can be
  // removed from the queue (since their weight is now invalid) and
  // then (only) their weight can be recalculated anew.
  map<RV*, multimap< vector<float>,RV*>::iterator > 
    rv2unNodesMap;

  // Also, create a set of nodes which are the ones whose weight
  // needs to be updated in the priority queue 'unorderedNodes'.
  // We begin by updating the weights of all nodes.
  set<RV*> nodesToUpdate = nodes;

  do {

    for (set<RV*>::iterator i = nodesToUpdate.begin();
	 i != nodesToUpdate.end();
	 i++) {

      infoMsg(Huge,"TR: computing weight of node %s(%d)\n",
	      (*i)->name().c_str(),(*i)->frame());

      // Create a vector with (weight,fillin,timeframe, etc.)
      // and choose in increasing lexigraphic order in that
      // tuple.
      vector<float> weight;

      // Create activeNeighbors, which contains only those neighbors
      // that are active and are not yet eliminated.
      set<RV*> activeNeighbors;
      set_difference((*i)->neighbors.begin(),(*i)->neighbors.end(),
		     orderedNodesSet.begin(),orderedNodesSet.end(),
		     inserter(activeNeighbors,activeNeighbors.end()));

      // pre-allocate and reserve enough space to avoid re-copies.
      weight.reserve(th_v.size());
      // go through and add each heurstic score to the weight array,
      // earlier scores in get higher priority.
      for (unsigned thi=0;thi<th_v.size();thi++) {

	const BasicTriangulateHeuristic th = th_v[thi];

	// 
	// TODO: add another heuristic, which is size^\beta * weight^\gamma
	//       where \gamma and \beta are command line parameters.
	// 

	if (th == TH_MIN_WEIGHT || th == TH_MIN_WEIGHT_NO_D) {
	  float tmp_weight = MaxClique::computeWeight(activeNeighbors,(*i),
					   (th == TH_MIN_WEIGHT));
	  weight.push_back(tmp_weight);
	  infoMsg(Huge,"  node has weight = %f\n",tmp_weight);
	} else if (th == TH_MIN_FILLIN) {
	  int fill_in = computeFillIn(activeNeighbors);
	  weight.push_back((float)fill_in);
	  infoMsg(Huge,"  node has fill_in = %d\n",fill_in);
	} else if (th == TH_WEIGHTED_MIN_FILLIN) {
	  double wfill_in = computeWeightedFillIn(activeNeighbors);
	  weight.push_back(wfill_in);
	  infoMsg(Huge,"  node has weighted_fill_in = %f\n",wfill_in);
	} else if (th == TH_MIN_TIMEFRAME) {
	  weight.push_back((*i)->frame());
	  infoMsg(Huge,"  node has time frame = %d\n",(*i)->frame());
	} else if (th == TH_MAX_TIMEFRAME) {
	  weight.push_back( - ((*i)->frame()));
	  infoMsg(Huge,"  node has (neg) time frame = -%d\n",(*i)->frame());
	} else if (th == TH_MIN_SIZE) {
	  weight.push_back((float)activeNeighbors.size());
	  infoMsg(Huge,"  node has active neighbor size = %d\n",
		  activeNeighbors.size());
	} else if (th == TH_MIN_POSITION_IN_FILE) {
	  weight.push_back((float)(*i)->rv_info.variablePositionInStrFile);
	  infoMsg(Huge,"  node has position in file = %d\n",
		  (*i)->rv_info.variablePositionInStrFile);
	} else if (th == TH_MIN_HINT) {
	  weight.push_back((float)(*i)->rv_info.eliminationOrderHint);
	  infoMsg(Huge,"  node has elimination order hint = %f\n",
		  (*i)->rv_info.eliminationOrderHint);
	} else if (th == TH_RANDOM) {
	  float tmp = rnd.drand48();
	  weight.push_back(tmp);
	  infoMsg(Huge,"  node has random value = %f\n",tmp);
	} else
	  warning("Warning: unimplemented triangulation heuristic (ignored)\n");
      }

      pair< vector<float>,RV*> p(weight,(*i));
      rv2unNodesMap[(*i)] = (unorderedNodes.insert(p));
    }

    if (message(Huge)) {
      // go through and print sorted guys.
      printf("Order of nodes to be eliminated\n");
      for (multimap< vector<float> ,RV*>::iterator m
	     = unorderedNodes.begin();
	   m != unorderedNodes.end(); m++) {
	printf("Node %s(%d) with weights:",
	       (*m).second->name().c_str(),
	       (*m).second->frame());
	for (unsigned l = 0; l < (*m).first.size(); l++ )
	  printf(" %f,",(*m).first[l]);
	printf("\n");
      }
    }

    // Go through the updated multi-map, grab an iterator pair to
    // iterate between all nodes that have the lowest weight. This
    // utilizes the fact that the multimap stores values in ascending
    // order based on key (in this case the weight), and so the first
    // one (i.e., mm.begin() ) should have the lowest weight.
    pair< 
      multimap< vector<float>,RV*>::iterator, 
      multimap< vector<float>,RV*>::iterator 
      > ip = unorderedNodes.equal_range( (*(unorderedNodes.begin())).first );

    const unsigned d = distance(ip.first,ip.second);

    // compute number of random top. We first take the min of
    // numRandomTop and number of remaining nodes, to make sure that
    // we don't randomly choose from a number with too many nodes. We
    // take the max of that with d, so that if more than numRandomTop
    // nodes tie, we still randomly choose from among all the tied
    // nodes.
    const unsigned curNumRandomTop = 
      max(min(numRandomTop,(unsigned)unorderedNodes.size()),d);

    if (curNumRandomTop == 1) {
      // then there is only one node and we eliminate that
      // so do nothing
    } else {
      // choose a random one, don't re-seed rng as that
      // should be done one time for the program via command line arguments.

      // TODO: add option so that rather choose one from the top N uniformly
      //       at random, we use some function of the weights (and perhaps position)
      //       to choose according to the (unnormalized) distribution given by these
      //       weights. This distribution should also be parameterized by \lambda so
      //       that when \lambda = 0, we get back to unform distribution, and \lambda = \infty
      //       we get back to picking the best (first) one. We can convert from teh
      //       vector to a single score by doing things like: a*1000 + b*100 + c*10 + d.
      //       We can use this once we normalize each score to be in range (0,10).

      RAND rnd(false);
      int val = rnd.uniform(curNumRandomTop-1);
      // TODO: there must be a better way to do this next step!
      while (val--)
	ip.first++;
    }

    // ip.first now points to the pair containing the random variable that
    // we eliminate.
    RV *rv = (*(ip.first)).second;

    if (message(Huge)) {
      printf("\nEliminating node %s(%d) with weights:",
	     rv->name().c_str(),rv->frame());
      for (unsigned l = 0; l < (*(ip.first)).first.size(); l++ )
	printf(" %f,",(*(ip.first)).first[l]);
      printf("\n");
    }


    // connect all neighbors of r.v. excluding nodes in 'orderedNodesSet'.
    rv->connectNeighbors(orderedNodesSet);

    // find the cliques if they are asked for.
    if (findCliques) {
      // check here if this node + its neighbors is a subset of
      // previous maxcliques. If it is not a subset of any previous
      // maxclique, then this node and its neighbors is a new
      // maxclique.
      set<RV*> candidateMaxClique;
      set_difference(rv->neighbors.begin(),rv->neighbors.end(),
		     orderedNodesSet.begin(),orderedNodesSet.end(),
		     inserter(candidateMaxClique,candidateMaxClique.end()));
      candidateMaxClique.insert(rv);
      bool is_max_clique = true;
      for (unsigned i=0;i < cliques.size(); i++) {
	if (includes(cliques[i].nodes.begin(),cliques[i].nodes.end(),
		     candidateMaxClique.begin(),candidateMaxClique.end())) {
	  // then found a 'proven' maxclique that includes our current
	  // candidate, so the candidate cannot be a maxclique
	  is_max_clique = false;
	  break;
	}
      }
      if (is_max_clique) {
	if (message(Huge)) {
	  // print out clique information.
	  printf("Found a max clique of size %ld while eliminating node %s(%d):",
		 (unsigned long)candidateMaxClique.size(),
		 rv->name().c_str(),rv->frame());
	  for (set<RV*>::iterator j=candidateMaxClique.begin();
	       j != candidateMaxClique.end(); j++) {
	    RV* rv = (*j);
	    printf(" %s(%d)",rv->name().c_str(), rv->frame());
	  }
	  printf("\n");
	}
 	cliques.push_back(MaxClique(candidateMaxClique));
      }
    }
      
    // insert node into ordered list
    orderedNodes.push_back(rv);

    // insert node into ordered set
    orderedNodesSet.insert(rv);

    // erase node from priority queue
    unorderedNodes.erase(ip.first);

    // only update not-yet-eliminated nodes that could possibly have
    // been effected by the current node 'rv' being eliminated. I.e.,
    // create the set of active neighbors of rv.
    nodesToUpdate.clear();
    set_difference(rv->neighbors.begin(),rv->neighbors.end(),
		   orderedNodesSet.begin(),orderedNodesSet.end(),
		   inserter(nodesToUpdate,nodesToUpdate.end()));

    // erase active neighbors of nodes since they will need to be
    // recomputed above.
    for (set<RV*>::iterator n = nodesToUpdate.begin();
	 n != nodesToUpdate.end();
	 n++) {
      unorderedNodes.erase(rv2unNodesMap[(*n)]);
    }

    // continue until all nodes are eliminated.
  } while (orderedNodesSet.size() < num_nodes);

  infoMsg(Huge,"\nENDING BASIC TRIANGULATION --- \n");
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::
 *  fillAccordingToCliques 
 *  
 * Preconditions:
 *  If one wants the graph to match the given cliques exactly, the 
 *  RandomVariables should not have any neighbor which are present in the 
 *  input cliques as this procedure does not remove any edges.  Typically 
 *  this procedure is preceeded by resotring the original graph neighbors 
 *  or removing all neighbors, and the vector of cliques corresponds to a
 *  triangulation.
 *
 * Postconditions:
 *   Graph is triangulated according to the best weight found.
 *   
 * Side Effects:
 *
 * Results:
 *     none
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
fillAccordingToCliques(
  const vector<MaxClique>& cliques
  ) 
{
  //////////////////////////////////////////////////////////////////////////
  // Convert to triangulate the RandomVariable structures
  //////////////////////////////////////////////////////////////////////////
  vector<MaxClique>::const_iterator crrnt_clique;
  vector<MaxClique>::const_iterator end_clique;

  for ( crrnt_clique = cliques.begin(), 
        end_clique   = cliques.end();
        crrnt_clique != end_clique;
        ++crrnt_clique ) {
    MaxClique::makeComplete((*crrnt_clique).nodes);
  }
}

/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::fillParentChildLists
 *   Fill containers in the triangulateNode structure that list each 
 *   node's parents and nonChildren.  nonChildren are the union of the 
 *   parents and undirected neighbors.  Note that these lists will be 
 *   different from the ones contained the associated RandomVariable 
 *   because the containers created here don't include nodes from outside 
 *   of the partition. 
 * 
 * Preconditions:
 *   The nodes given as input are initialized 
 *
 * Postconditions:
 *   The parents and nonChildren containers of each node are filled in.
 * 
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
fillParentChildLists(
  vector<triangulateNode>& nodes,
  bool includeUndirected 
  )
{
  vector<triangulateNode>::iterator crrnt_node;
  vector<triangulateNode>::iterator end_node;
  triangulateNeighborType::iterator crrnt_nghbr;
  triangulateNeighborType::iterator end_nghbr;
  vector<RV*>::iterator found_node;

  for( crrnt_node = nodes.begin(),
       end_node   = nodes.end();
       crrnt_node != end_node;
       ++crrnt_node ) {

    for( crrnt_nghbr = (*crrnt_node).neighbors.begin(), 
         end_nghbr   = (*crrnt_node).neighbors.end(); 
         crrnt_nghbr != end_nghbr;
         ++crrnt_nghbr ) {

      found_node = find( 
        (*crrnt_node).randomVariable->allParents.begin(), 
        (*crrnt_node).randomVariable->allParents.end(),
        (*crrnt_nghbr)->randomVariable );

      //////////////////////////////////////////////////////////////
      // If a parent, add to both parents and non-children
      //////////////////////////////////////////////////////////////
      if (found_node != 
          (*crrnt_node).randomVariable->allParents.end()) {
        (*crrnt_node).parents.push_back( *crrnt_nghbr );
        (*crrnt_node).nonChildren.push_back( *crrnt_nghbr );
      }
      //////////////////////////////////////////////////////////////
      // If not a parent, check if it is a child
      //////////////////////////////////////////////////////////////
      else if (includeUndirected) {
        found_node = find( 
          (*crrnt_node).randomVariable->allChildren.begin(), 
          (*crrnt_node).randomVariable->allChildren.end(),
          (*crrnt_nghbr)->randomVariable ); 

        if (found_node == (*crrnt_node).randomVariable->allChildren.end()) {
          (*crrnt_node).nonChildren.push_back( *crrnt_nghbr );
        }
      }
    }
  } 

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::addExtraEdgesToGraph()
 *   Add extra edges to the graph according to a particular heuristic
 *  
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The nodes may have additional neighbors.
 * 
 * Side Effects:
 *   The parents and nonChildren members of the triangulateNode 
 *   structures are filled in. 
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
addExtraEdgesToGraph(
		     const set<RV*>&  nodes,  
		     const extraEdgeHeuristicType edge_heuristic 
		     )
{
  vector<triangulateNode>  triangulate_nodes;

  fillTriangulateNodeStructures( nodes, triangulate_nodes );
  addExtraEdgesToGraph( triangulate_nodes, edge_heuristic );
}

/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::addExtraEdgesToGraph()
 *   Add extra edges to the graph according to a particular heuristic
 *  
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The nodes may have additional neighbors.
 * 
 * Side Effects:
 *   The parents and nonChildren members of the triangulateNode 
 *   structures are filled in. 
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
addExtraEdgesToGraph(
  vector<triangulateNode>&     nodes,
  const extraEdgeHeuristicType edge_heuristic 
  )
{
  vector<triangulateNode>::iterator crrnt_node;
  vector<triangulateNode>::iterator end_node;
  vector<triangulateNode>::iterator crrnt_mark;
  vector<triangulateNode>::iterator end_mark;
  triangulateNeighborType::iterator crrnt_nghbr;
  triangulateNeighborType::iterator end_nghbr;
  vector<edge> extra_edges;


  if (edge_heuristic == SOME_EDGES) {
    fillParentChildLists( nodes, false );
  }
  else {
    fillParentChildLists( nodes, true );
  }

  //////////////////////////////////////////////////////////////////////////
  // Iterate through all nodes
  //////////////////////////////////////////////////////////////////////////
  for( crrnt_node = nodes.begin(),
       end_node   = nodes.end();
       crrnt_node != end_node;
       ++crrnt_node ) {

    //////////////////////////////////////////////////////////////////////////
    // Mark all nodes as not visited 
    //////////////////////////////////////////////////////////////////////////
    for( crrnt_mark = nodes.begin(),
         end_mark   = nodes.end();
         crrnt_mark != end_mark;
         ++crrnt_mark ) {
      (*crrnt_mark).marked = false;
    }

    (*crrnt_node).marked = true;

    //////////////////////////////////////////////////////////////////////////
    // Find all nodes which are parents or undirected neighbors, and are 
    //   deterministic or sparse 
    //////////////////////////////////////////////////////////////////////////
    for( crrnt_nghbr = (*crrnt_node).nonChildren.begin(), 
         end_nghbr   = (*crrnt_node).nonChildren.end(); 
         crrnt_nghbr != end_nghbr;
         ++crrnt_nghbr ) {

      (*crrnt_nghbr)->marked = true;

      if  ((*crrnt_nghbr)->parents.size() > 0)  { 
        if ( 
             (((*crrnt_nghbr)->randomVariable->discrete()) && 
              (((DiscRV*)
		((*crrnt_nghbr)->randomVariable))->deterministic()))
	     || 
             (((*crrnt_nghbr)->randomVariable->discrete()) && 
              (((DiscRV*)
		((*crrnt_nghbr)->randomVariable))->sparse()))
	     ) 
	  {

          addEdgesToNode((*crrnt_nghbr)->parents, *crrnt_nghbr, &(*crrnt_node), 
            edge_heuristic, extra_edges);
        }
      }
    }
  } 

  addEdges(extra_edges);
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::addEdgesToNode
 *   Choose if an edge should be added from a set of nodes to another 
 *   node.  This is intended to be a set of parents of child connecting  
 *   to a grandchild for use in addExtraEdgesToGraph.  If the edge is 
 *   added, this procedure calls itself recursively. 
 *  
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 * 
 * Postconditions:
 *   The choice of edges is stored in 'extra_edges'.  These will be from 
 *   'grandchild' to any other node in the graph.  The neighbor sets of 
 *   the nodes are not modified.  
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
addEdgesToNode(
  triangulateNeighborType&     parent_set,  
  triangulateNode* const       child, 
  triangulateNode* const       grandchild,
  const extraEdgeHeuristicType edge_heuristic, 
  vector<edge>&                extra_edges
  )
{
  triangulateNeighborType::iterator crrnt_prnt;
  triangulateNeighborType::iterator end_prnt;
  float parent_weight, child_weight; 
  float no_edge_weight, with_edge_weight;  
  set<RV*> parents_child, child_grandchild, all;  
  RAND rndm_nmbr(0);
  bool add_edge = false;

  //////////////////////////////////////////////////////////////////////////
  // Choose if the edges should be added according to edge_heuristic
  //////////////////////////////////////////////////////////////////////////
  switch (edge_heuristic) {

    case ALL_EDGES:
    case SOME_EDGES:
      add_edge = true;
      break;

    case RANDOM_EDGES:
      add_edge = rndm_nmbr.uniform( 1 ); 
      break;

    case LOCALLY_OPTIMAL_EDGES:
      for( crrnt_prnt = parent_set.begin(), end_prnt = parent_set.end();
           crrnt_prnt != end_prnt;
         ++crrnt_prnt ) {
        parents_child.insert( (*crrnt_prnt)->randomVariable );
      }
      parents_child.insert( child->randomVariable );

      child_grandchild.insert( child->randomVariable );
      child_grandchild.insert( grandchild->randomVariable );

      all = parents_child;
      all.insert( grandchild->randomVariable );

      parent_weight = MaxClique::computeWeight(parents_child);  
      child_weight  = MaxClique::computeWeight(child_grandchild);

      no_edge_weight = log10add(parent_weight,child_weight);
      with_edge_weight = MaxClique::computeWeight(all); 
      if (with_edge_weight < no_edge_weight) { 
        add_edge = true;
      }
      else {
        add_edge = false;
      }
      break;
  
    default:
      assert(0);
      break;
  }

  //////////////////////////////////////////////////////////////////////////
  // Add the edges if the criteria was met 
  //////////////////////////////////////////////////////////////////////////
  if (add_edge) {

    //////////////////////////////////////////////////////////////////////////
    // Add the edge from each node in parent_set to grandchild 
    //////////////////////////////////////////////////////////////////////////
    for( crrnt_prnt = parent_set.begin(), end_prnt = parent_set.end();
         crrnt_prnt != end_prnt;
         ++crrnt_prnt ) {

      extra_edges.push_back( edge(*crrnt_prnt, grandchild) ); 
      (*crrnt_prnt)->marked = true;

      ////////////////////////////////////////////////////////////////////////
      // The current parent just gained an undirected neighbor, so recurse if 
      // the parent is deterministic or sparse. 
      ////////////////////////////////////////////////////////////////////////
      if  ((*crrnt_prnt)->parents.size() > 0)  { 
        if ( 
             (((*crrnt_prnt)->randomVariable->discrete()) && 
              (((DiscRV*)
               ((*crrnt_prnt)->randomVariable))->deterministic()))
	    || 
             (((*crrnt_prnt)->randomVariable->discrete()) && 
              (((DiscRV*)
               ((*crrnt_prnt)->randomVariable))->sparse())) 
	     ) {
          addEdgesToNode( (*crrnt_prnt)->parents, *crrnt_prnt, grandchild, 
            edge_heuristic, extra_edges );        
        }
      }
    }
  }

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::addEdges
 *   Add neighbors to RandomVariables according to a vector edge 
 *   structures.  Node that the edge class contains triangulateNode's, 
 *   but this proceedure adds the neighbors to the associated 
 *   RandomVariables. 
 * 
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   The neighbors of the RandomVariable associated with each 
 *   triangulateNode in each edge will change. 
 *   
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
addEdges(
  const vector<edge>& extra_edges
  )
{
  vector<edge>::const_iterator crrnt_edge; 
  vector<edge>::const_iterator end_edge; 

  for ( crrnt_edge = extra_edges.begin(),
        end_edge   = extra_edges.end();
        crrnt_edge != end_edge;
        ++crrnt_edge ) {

    (*crrnt_edge).first()->randomVariable->neighbors.insert(
      (*crrnt_edge).second()->randomVariable);
    (*crrnt_edge).second()->randomVariable->neighbors.insert(
      (*crrnt_edge).first()->randomVariable);
  }
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateSimulatedAnnealing()
 *   A stochastic search of elimination orderings.
 *  
 *   Simulated  annealing randomly permutes two nodes in the elimination 
 *   ordering, this move is accepted if the weight is improved and 
 *   accepted with some probability proportional to a "temperature" 
 *   parameter.  This function controls the change of the temperature 
 *   parameter, and the random permutations are handled by annealChain.   
 *
 *   The temperature begins high enough that most moves are accepted.  
 *   The temperature is gradually lowered so that fewer and fewer 
 *   ascent moves are accepted.  The temperature is lowered by the 
 *   schedule given by Aarts and Laarhoven:
 *  
 * crrnt_tmprtr=crrnt_tmprtr*(1+(crrnt_tmprtr*log(1+distance))/(3*std_dev))^-1
 *
 *   Annealing finishes when no moves are accepted in a chain or when the 
 *   stop criterion given by Varanelli is fulfilled: 
 *
 *     ((mean - best_this_weight) / std_dev) < stop_ratio
 *
 *   References:
 *   E.H.L Aarts and P.J.M. van Laarhoven, "A New Polynomial-Time Cooling 
 *   Schedule," Proc.  IEEE ICCAD-85, Santa Clara, CA, 206-208, 1985.
 *
 *   U. Kj�rulff.  "Optimal decomposition of probabilistic networks by 
 *   simulated annealing." Statistics and Computing, 2(7-17), 1992.
 *
 *   J. Varanelli, "On the Acceleration of Simulated Annealing," PhD thesis,
 *   University of Virginia, Department of Computer Science
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 * 
 * Postconditions:
 *   The graph is triangulated to the lowest weight triangulation found
 *   at any point during the search.  The corresponding order is stored 
 *   in best_order.
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateSimulatedAnnealing(
  const set<RV*>& nodes,
  const bool jtWeight,
  const set<RV*>& nodesRootMustContain,
  vector<MaxClique>&          best_cliques,
  vector<RV*>&    best_order,
  string&                     parameter_string 
  )
{
  ///////////////////////////////////////////////////////////////////////
  // Simulated annealing parameters:
  //
  // distance - controls amount of change in annealing temperature, a 
  //   smaller value gives a slower schedule 
  //     
  // stop_ratio - controls when annealing stops, a smaller value allows 
  //   annealing to run longer 
  //     
  // chain_length - number of permutations to try before lowering the 
  //   temperature.  A larger number brings annealing closer to a steady
  //   state distribution at each temperature, a smaller number anneals 
  //   faster.
  ///////////////////////////////////////////////////////////////////////
  const double   distance = 0.01;
  const double   stop_ratio = 0.0001;
  const unsigned chain_length = 1000;

  ///////////////////////////////////////////////////////////////////////
  // Record parameters in a comment string
  ///////////////////////////////////////////////////////////////////////
  enum {
    string_length = 16
  };
  char parameter[string_length];    

  sprintf( parameter, "%f", distance );
  assert( strlen(parameter)<string_length ); 
  parameter_string = "Distance:" + string(parameter); 
  sprintf( parameter, "%f", stop_ratio);
  assert( strlen(parameter)<string_length ); 
  parameter_string = parameter_string + "  Stop Ratio:" + string(parameter); 
  sprintf( parameter, "%d", chain_length);
  assert( strlen(parameter)<string_length ); 
  parameter_string = parameter_string + "  Chain Length:" + string(parameter); 
  
  ///////////////////////////////////////////////////////////////////////
  // Local Variables
  ///////////////////////////////////////////////////////////////////////
  vector<triangulateNode>          triangulate_nodes;
  vector<triangulateNode*>         crrnt_order;
  vector<triangulateNode*>         triangulate_best_order;
  list<vector<triangulateNode*> >  cliques;
  vector<MaxClique>                rv_cliques;
  vector<triangulateNode*>         dummy_order;
  vector<triangulateNghbrPairType> orgnl_nghbrs;

  back_insert_iterator<vector<triangulateNode*> > bi_crrnt(crrnt_order);
  back_insert_iterator<vector<triangulateNode*> > 
    bi_best(triangulate_best_order);
  vector<triangulateNode>::iterator  crrnt_node;  
  vector<triangulateNode>::iterator  end_node;  
  vector<triangulateNode*>::iterator crrnt_np;  
  vector<triangulateNode*>::iterator end_np;  

  double     crrnt_tmprtr;        // Current annealing temperature

  double     best_graph_weight;   // Best overal graph weight
  double     best_this_weight;    // Best graph weight on most recent trial

  double     weight_sum = 0;      // Sum of weights (for variance calculation)
  double     weight_sqr_sum = 0;  // Sum of weights^2 (for variance calculation)
  double     mean, variance, std_dev;

  double     ratio = 0;           // Stop ratio for current run 

  unsigned   i;
  unsigned   moves_accepted;

  bool       exit;

  ///////////////////////////////////////////////////////////////////
  // Initialize data structures 
  ///////////////////////////////////////////////////////////////////
  infoMsg(IM::Tiny, "Annealing:\n");

  fillTriangulateNodeStructures( nodes, triangulate_nodes );

  saveCurrentNeighbors( triangulate_nodes, orgnl_nghbrs );  

  for( crrnt_node = triangulate_nodes.begin(), 
       end_node   = triangulate_nodes.end(); 
       crrnt_node != end_node; 
       ++crrnt_node ) { 

    crrnt_order.push_back( &(*crrnt_node) );    
  }

  ///////////////////////////////////////////////////////////////////
  // Begin with random elimination order  
  ///////////////////////////////////////////////////////////////////
  random_shuffle (crrnt_order.begin(), crrnt_order.end());
  copy (crrnt_order.begin(), crrnt_order.end(), bi_best);

  fillInComputation( crrnt_order );
  maximumCardinalitySearch( triangulate_nodes, cliques, dummy_order, false );

  listVectorCliquetoVectorSetClique( cliques, best_cliques );
  best_graph_weight = graphWeight(best_cliques,jtWeight,nodesRootMustContain);
  weight_sum = best_graph_weight;
  weight_sqr_sum = best_graph_weight*best_graph_weight;

  infoMsg(IM::Tiny, "  Starting weight:  %f\n", best_graph_weight); 

  ///////////////////////////////////////////////////////////////////
  // Run a markov chain at high temperature to get statistics 
  // for starting temperature. 
  ///////////////////////////////////////////////////////////////////

  moves_accepted = annealChain(
    triangulate_nodes, 
    jtWeight,
    nodesRootMustContain,
    crrnt_order,
    triangulate_best_order,
    best_graph_weight,
    best_this_weight,
    DBL_MAX,
    chain_length,
    weight_sum,         
    weight_sqr_sum,
    orgnl_nghbrs);         

  mean = weight_sum/moves_accepted; 
  ++moves_accepted; 
  variance = (weight_sqr_sum-(weight_sum*weight_sum)/(double)moves_accepted)/
              (double)moves_accepted;
  if (variance < 0) {
    variance = 0;
  }
  std_dev = sqrt(variance);

  crrnt_tmprtr = 1.0+std_dev;

  infoMsg(IM::Tiny, "  Initial Temperature: %f\n", crrnt_tmprtr);
  infoMsg(IM::Tiny, "  Distance:   %f\n", distance );
  infoMsg(IM::Tiny, "  Stop Ratio: %f\n", stop_ratio);
 
  ////////////////////////////////////////////////////////////
  // Begin with best order 
  ////////////////////////////////////////////////////////////
  crrnt_order.clear();
  copy( triangulate_best_order.begin(), triangulate_best_order.end(), 
    bi_crrnt );

  ////////////////////////////////////////////////////////////
  // Loop until stop condition found 
  ////////////////////////////////////////////////////////////
  exit = false;
  i = 0;
  do {
    ////////////////////////////////////////////////////////////////
    // Run a markov chain at current temperature 
    ////////////////////////////////////////////////////////////////
    weight_sum = 0;
    weight_sqr_sum = 0;

    moves_accepted = annealChain(
		       triangulate_nodes, 
		       jtWeight,
		       nodesRootMustContain,
                       crrnt_order,
                       triangulate_best_order,
                       best_graph_weight,
                       best_this_weight,
                       crrnt_tmprtr,
                       chain_length,
                       weight_sum,         
                       weight_sqr_sum,
		       orgnl_nghbrs);         

    ////////////////////////////////////////////////////////////////
    // Lower the temperature and calculate current stop ratio 
    ////////////////////////////////////////////////////////////////
    if (moves_accepted > 0) {

      mean = weight_sum/moves_accepted; 
      ++moves_accepted; 
      variance = ( weight_sqr_sum - (weight_sum*weight_sum)/
                   (double)moves_accepted ) / (double)moves_accepted;
      if (variance < 0) {
        variance = 0;
      }

      std_dev = sqrt(variance);

      if (std_dev > DBL_MIN) {
        crrnt_tmprtr = crrnt_tmprtr /
          (1 + (crrnt_tmprtr*log(1+distance))/(3*std_dev));  

        ratio = (mean - best_this_weight) / std_dev; 
        ++i;
      }
      else {
        ratio = 0;
      }
    }
    ////////////////////////////////////////////////////////////////
    // Exit prematurely if no moves in the chain were accepted 
    ////////////////////////////////////////////////////////////////
    else { 
      exit = true; 
    }

    ////////////////////////////////////////////////////////////////
    // Exit prematurely if the timer has expired 
    ////////////////////////////////////////////////////////////////
    if (timer && timer->Expired()) {
      exit = true;
      infoMsg(IM::Tiny, "Exiting Annealing Because Time Has Expired: %d\n", i); 
    }  

  } while ((! exit) && (ratio > stop_ratio));

  ////////////////////////////////////////////////////////////////
  // Exit with the best triangulated graph found 
  ////////////////////////////////////////////////////////////////
  infoMsg(IM::Tiny, "  Exiting on iteration: %d\n", i); 

  best_order.clear();
  for( crrnt_np = triangulate_best_order.begin(), 
       end_np   = triangulate_best_order.end(); 
       crrnt_np != end_np; 
       ++crrnt_np ) {

    best_order.push_back( (*crrnt_np)->randomVariable ); 
  }

  best_cliques.clear();
  triangulateElimination( nodes, best_order, best_cliques);
  infoMsg(IM::Tiny, "  Annealing Best Weight: %f\n", best_graph_weight ); 
}


/*-
 *-----------------------------------------------------------------------
 * annealChain 
 *   Use annealing for a Markov Chain of specified length at a constant
 *   temperature.  Helper function to triangulateSimulatedAnnealing.
 *   The chain is generated by purmutation two of the nodes in the 
 *   elimination order, the move is accepted if it results in a lower 
 *   weight and the move is accepted with some probability if it results
 *   in a higher weight.  
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   crrnt_order       - the last order of the last accepted perumutation
 *   best_order        - the order giving the best weight found 
 *   best_graph_weight - the best graph weight found overall
 *   best_this_weight  - the best graph weight found in this chain
 *   weight_sum        - sum of all the weights of accepted triangulations 
 *   weight_sqr_sum    - sum of all the weight^2 of accepted triangulations 
 *
 * Side Effects:
 *   The partition is triangulated to the last triangulation in the 
 *   current order.  Neighbor members of each random variable can be 
 *   changed.
 *
 * Results:
 *   The number of permutations that were accepted 
 *
 *-----------------------------------------------------------------------
 */
unsigned
BoundaryTriangulate::
annealChain(
  vector<triangulateNode>&          nodes,
  const bool jtWeight,
  const set<RV*>& nodesRootMustContain,
  vector<triangulateNode*>&         crrnt_order,
  vector<triangulateNode*>&         best_order,
  double&                           best_graph_weight,
  double&                           best_this_weight,
  double                            temperature,
  unsigned                          iterations,
  double&                           weight_sum,         
  double&                           weight_sqr_sum,         
  vector<triangulateNghbrPairType>& orgnl_nghbrs
  )
{  
  vector<MaxClique>               rv_cliques;
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        dummy_order;

  back_insert_iterator<vector<triangulateNode*> > bi_best(best_order);

  triangulateNode* tmp_node; 
  RAND             rndm_nmbr(0);
  double           crrnt_graph_weight;
  double           prvs_graph_weight;
  double           tmprtr_penalty;
  unsigned         first_index  = 0;    
  unsigned         second_index = 0;    
  unsigned         moves_accepted;
  unsigned         i;
  bool             accepted;
  
  crrnt_graph_weight = DBL_MAX;
  prvs_graph_weight  = DBL_MAX;
  best_this_weight   = DBL_MAX;
  moves_accepted     = 0;

  ///////////////////////////////////////////////////////////////////////
  // Permute nodes 'iterations' times  
  ///////////////////////////////////////////////////////////////////////
  for(i=0; i<iterations; ++i) {

    ////////////////////////////////////////////////////////////////
    // Randomly swap two nodes 
    ////////////////////////////////////////////////////////////////
    if (crrnt_order.size() >= 2) {
      do { 
        first_index = rndm_nmbr.uniform( crrnt_order.size() - 1 );
        assert( first_index < crrnt_order.size() );
        second_index = rndm_nmbr.uniform( crrnt_order.size() - 1 );
        assert( second_index < crrnt_order.size() );
      } while (first_index == second_index);

      tmp_node = crrnt_order[first_index];
      crrnt_order[first_index]  = crrnt_order[second_index];
      crrnt_order[second_index] = tmp_node; 
    } 

    ////////////////////////////////////////////////////////////////
    // Calculate new graph weight 
    ////////////////////////////////////////////////////////////////
    restoreNeighbors(orgnl_nghbrs);
    fillInComputation( crrnt_order );
    maximumCardinalitySearch( nodes, list_cliques, dummy_order, false );
    listVectorCliquetoVectorSetClique( list_cliques, rv_cliques );
    crrnt_graph_weight = graphWeight(rv_cliques,jtWeight,nodesRootMustContain);

    ////////////////////////////////////////////////////////////////
    // Check if it is the best ordering so far 
    ////////////////////////////////////////////////////////////////
    if (crrnt_graph_weight < best_graph_weight) {
      best_graph_weight = crrnt_graph_weight;

      best_order.clear();
      copy(crrnt_order.begin(), crrnt_order.end(), bi_best);
    }

    ////////////////////////////////////////////////////////////////
    // If new weight is better always accept it.  If weight is 
    //  worse, accept it with a probability calculated from the 
    //  temperature. 
    ////////////////////////////////////////////////////////////////
    accepted = true;
    if ((crrnt_graph_weight-prvs_graph_weight) > 1e-8) {
      tmprtr_penalty = temperature * log(rndm_nmbr.drand48());

      if ( crrnt_graph_weight > (prvs_graph_weight-tmprtr_penalty)) {
        tmp_node = crrnt_order[first_index];
        crrnt_order[first_index]  = crrnt_order[second_index];
        crrnt_order[second_index] = tmp_node;
        accepted = false;
      }
    } 

    if (accepted == true) {
      prvs_graph_weight = crrnt_graph_weight;
      weight_sum += prvs_graph_weight;
      weight_sqr_sum += prvs_graph_weight*prvs_graph_weight;
      ++moves_accepted;
    }
  
    if (prvs_graph_weight < best_this_weight) {
      best_this_weight = prvs_graph_weight; 
    }
  }
 
  return(moves_accepted);
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateCrossover() 
 *   Takes two input triangulations and creates two triangulations that are
 *   hybrids of the two input triangulations.  To do this it iterates through
 *   each edge missing from the original graph, if the edge is in or is missing
 *   from both graphs it does nothing, otherwise it switches them with
 *   crossoverProbability.  If the new graphs are not triangulated they are
 *   triangulated using maximum cardinality search.  After both new
 *   triangulations have been created it performs a mutation (a one-edge
 *   triangulation) with mutateProbability.
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set.   
 *  
 * Postconditions:
 *   The graphs in the given partitions have new triangulations.
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none 
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateCrossover(
  GMTemplate& t1, 
  vector<MaxClique>& t1_P,
  vector<MaxClique>& t1_C,
  vector<MaxClique>& t1_E,
  GMTemplate& t2, 
  vector<MaxClique>& t2_P,
  vector<MaxClique>& t2_C,
  vector<MaxClique>& t2_E,
  float crossoverProbability,
  float mutateProbability,
  bool reTriP, 
  bool reTriC,
  bool reTriE )
{
  char crossoverName[2048];

  sprintf(crossoverName, "crossover-%4.2e-%4.2e", crossoverProbability, 
    mutateProbability);

  if (reTriP)
  {
    t1.P.cliques = t1_P;
    t2.P.cliques = t2_P;
  }

  if (reTriC)
  {
    t1.C.cliques = t1_C;
    t2.C.cliques = t2_C;
  }

  if (reTriE)
  {
    t1.E.cliques = t1_E;
    t2.E.cliques = t2_E;
  }

  t1.triangulatePartitionsByCliqueCompletion();
  t2.triangulatePartitionsByCliqueCompletion();

  if (reTriP)
  {
    triangulateCrossover( t1.P, t2.P, crossoverProbability, mutateProbability );
    t1.P.triMethod = crossoverName;
    t2.P.triMethod = crossoverName;
  }

  if (reTriC)
  {
    triangulateCrossover( t1.C, t2.C, crossoverProbability, mutateProbability );
    t1.C.triMethod = crossoverName;
    t2.C.triMethod = crossoverName;
  }

  if (reTriE)
  {
    triangulateCrossover( t1.E, t2.E, crossoverProbability, mutateProbability ); 
    t1.E.triMethod = crossoverName;
    t2.E.triMethod = crossoverName;
  }

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateCrossover() 
 *   Takes two input triangulations and creates two triangulations that are
 *   hybrids of the two input triangulations.  To do this it iterates through
 *   each edge missing from the original graph, if the edge is in or is missing
 *   from both graphs it does nothing, otherwise it switches them with
 *   crossoverProbability.  If the new graphs are not triangulated they are
 *   triangulated using maximum cardinality search.  After both new
 *   triangulations have been created it performs a mutation (a one-edge
 *   triangulation) with mutateProbability.  
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set.   
 *
 * Postconditions:
 *   The graphs in the given partitions have new triangulations.
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none 
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateCrossover(
  Partition& triangulation1, 
  Partition& triangulation2,
  float crossoverProbability,
  float mutateProbability
 )
{
  vector<RV*> order;
  SavedGraph orgnl_triangulation1_nghbrs;
  SavedGraph orgnl_triangulation2_nghbrs;
  set<RV*>::iterator t1_node_1;
  set<RV*>::iterator t1_node_2;
  set<RV*>::iterator t1_end_node;
  set<RV*>::iterator t2_node_1;
  set<RV*>::iterator t2_node_2;
  set<RV*>::iterator t2_end_node;
  set<RV*>::iterator t1_found_edge;
  set<RV*>::iterator t2_found_edge;
 
  saveCurrentNeighbors(triangulation1, orgnl_triangulation1_nghbrs);
  saveCurrentNeighbors(triangulation2, orgnl_triangulation2_nghbrs);

  t1_end_node = triangulation1.nodes.end();
  t2_end_node = triangulation2.nodes.end(); 

  for (t1_node_1 = triangulation1.nodes.begin();
       t1_node_1 != t1_end_node;
       t1_node_1++) {

    ////////////////////////////////////////////////////////////////////////
    // Find pointer to node in triangulation2 that is the same as t1_node_1
    ////////////////////////////////////////////////////////////////////////
    for (t2_node_1 = triangulation2.nodes.begin();
         t2_node_1 != t2_end_node;
         t2_node_1++ ) {

      if ((&((*t2_node_1)->rv_info) == (&(*t1_node_1)->rv_info)) &&
            ((*t2_node_1)->timeFrame == (*t1_node_1)->timeFrame)) { 
        break; 
      }
    }

    if (t2_node_1 == t2_end_node) {
      error("ERROR: '%s(%d)' exists in inputTriangulatedFile but can not be found in triangulation2TriagulatedFile", (*t1_node_1)->name().c_str(), (*t1_node_1)->frame() );
    }

    for (t1_node_2 = t1_node_1; 
         t1_node_2 != t1_end_node;
         t1_node_2++) {
       
      ////////////////////////////////////////////////////////////////////////
      // Find pointer to node in triangulation2 that is the same as t1_node_2
      ////////////////////////////////////////////////////////////////////////
      for (t2_node_2 = triangulation2.nodes.begin();
           t2_node_2 != t2_end_node;
           t2_node_2++ ) {

        if ((&((*t2_node_2)->rv_info) == (&(*t1_node_2)->rv_info)) &&
              ((*t2_node_2)->timeFrame == (*t1_node_2)->timeFrame)) { 
          break; 
        }
      }

      if (t2_node_2 == t2_end_node) {
        error("ERROR: '%s(%d)' exists in inputTriangulatedFile but can not be found in triangulation2TriagulatedFile", (*t1_node_2)->name().c_str(), (*t1_node_2)->frame() );
      }

      ////////////////////////////////////////////////////////////////////////
      // Find if node_1 and node_2 are neighbors 
      ////////////////////////////////////////////////////////////////////////
      t1_found_edge = find( (*t1_node_1)->neighbors.begin(), 
        (*t1_node_1)->neighbors.end(), *t1_node_2 ); 
      t2_found_edge = find( (*t2_node_1)->neighbors.begin(), 
        (*t2_node_1)->neighbors.end(), *t2_node_2 ); 

      //////////////////////////////////////////////////////////////////////
      // Edge not in t1, edge is in t2 
      //////////////////////////////////////////////////////////////////////
      if ((t1_found_edge == (*t1_node_1)->neighbors.end()) && 
          (t2_found_edge != (*t2_node_1)->neighbors.end())) { 

        if (rnd.drand48() < crossoverProbability) {
          (*t1_node_1)->neighbors.insert( *t1_node_2 );
          (*t1_node_2)->neighbors.insert( *t1_node_1 );

          (*t2_node_1)->neighbors.erase( *t2_node_2 );
          (*t2_node_2)->neighbors.erase( *t2_node_1 );
        }
      }
      //////////////////////////////////////////////////////////////////////
      // Edge is in t1, edge is not in t2 
      //////////////////////////////////////////////////////////////////////
      else if ((t1_found_edge != (*t1_node_1)->neighbors.end()) && 
               (t2_found_edge == (*t2_node_1)->neighbors.end())) { 

        if (rnd.drand48() < crossoverProbability) {
          (*t1_node_1)->neighbors.erase( *t1_node_2 );
          (*t1_node_2)->neighbors.erase( *t1_node_1 );

          (*t2_node_1)->neighbors.insert( *t2_node_2 );
          (*t2_node_2)->neighbors.insert( *t2_node_1 );
        }
      } 

    } 
  } 

  //////////////////////////////////////////////////////////////////////
  // Triangulate new graphs if not already triangulated 
  //////////////////////////////////////////////////////////////////////
  triangulateMaximumCardinalitySearch( triangulation1.nodes, 
    triangulation1.cliques, order ); 
  triangulateMaximumCardinalitySearch( triangulation2.nodes, 
    triangulation2.cliques, order ); 

  //////////////////////////////////////////////////////////////////////
  // Mutate triangulation given mutateProbability
  //////////////////////////////////////////////////////////////////////
  if (rnd.drand48() < mutateProbability) {
    triangulateOneEdgeChange( triangulation1, orgnl_triangulation1_nghbrs );
  }

  if (rnd.drand48() < mutateProbability) {
    triangulateOneEdgeChange( triangulation2, orgnl_triangulation2_nghbrs );
  }
 
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateOneEdgeChange
 *   Takes an input triangulation and creates a triangulations differs by one
 *   edge. 
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set.  Graph should be triangulated.   
 *
 * Postconditions:
 *   The graphs in the given partitions have new triangulations.
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none 
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateOneEdgeChange(
  Partition& part,
  SavedGraph orgnl_graph 
  )
{
  vector<triangulateNode> triangulate_nodes;
  vector<edge>            fill_in;
  vector<edge>            missing;
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        order;
  set<RV*>::iterator crrnt_RV;
  edge change_edge;
  bool edge_added;

  ////////////////////////////////////////////////////////////////////// 
  // Prepare 
  ////////////////////////////////////////////////////////////////////// 
  if (! chordalityTest(part.nodes) ) {
      error("ERROR: No input triangulation or input not triangulated");
  }

  fillTriangulateNodeStructures( part.nodes, triangulate_nodes );

  calculateMissingEdges( triangulate_nodes, missing );
  calculateFillInEdges( triangulate_nodes, orgnl_graph, fill_in );

  ////////////////////////////////////////////////////////////////////// 
  // Actually change the edge
  ////////////////////////////////////////////////////////////////////// 
  edge_added = changeOneEdge( triangulate_nodes, fill_in, missing, change_edge, 
    list_cliques ); 

  if (edge_added) {
    infoMsg(IM::Tiny, "Edge Added\n");
  }
  else {
    infoMsg(IM::Tiny, "Edge Subtracted\n");
  }

  ////////////////////////////////////////////////////////////////////// 
  // Convert new triangulation to standard cliques 
  ////////////////////////////////////////////////////////////////////// 
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false );
  assert( testZeroFillIn(order) ); 
  listVectorCliquetoVectorSetClique( list_cliques, part.cliques );

  if (part.cliques.size() > 0) {
    for ( crrnt_RV = part.nodes.begin();
          crrnt_RV != part.nodes.end();
          ++crrnt_RV ) {
      (*crrnt_RV)->neighbors.clear();
    }
    fillAccordingToCliques( part.cliques );
  }

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateRandom()
 *   Gives random triangulation.  This should give uniform probability
 *   over all possible triangulations (not just random over elimination 
 *   orders).  It does this by running a MCMC chain of triangulations 
 *   which differ by one edge.  This proceedure is a bit slow. 
 *  
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 * 
 * Postconditions:
 *   The triangulation with the lowest weight triangulation found is 
 *   stored in best_cliques.  The edges in the graph are left in an
 *   undetermined state. 
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateRandom(
  const set<RV*>&        nodes,
  vector<MaxClique>&     best_cliques
  )
{
  const unsigned chain_length = 10000;

  ///////////////////////////////////////////////////////////////////
  // Local Variables 
  ///////////////////////////////////////////////////////////////////
  vector<triangulateNode>         triangulate_nodes;
  vector<triangulateNode*>        order;
  list<vector<triangulateNode*> > cliques;
  vector<edge>                    fill_in;
  vector<edge>                    missing;
  vector<triangulateNghbrPairType> dummy_triangulation;
  const bool                       jtWeight = false;
  const set<RV*>                   nodesRootMustContain;

  vector<triangulateNode>::iterator crrnt_node; 
  vector<triangulateNode>::iterator end_node;

  double     best_graph_weight = DBL_MAX;   // Best overal graph weight
  double     best_this_weight;    // Best graph weight on most recent trial
#if 0
  // unused
  unsigned   moves_accepted;
#endif
  double     weight_sum = 0;      // Sum of weights (for variance calculation)
  double     weight_sqr_sum = 0;  // Sum of weights^2 (for variance calculation)

  fillTriangulateNodeStructures( nodes, triangulate_nodes );

  ///////////////////////////////////////////////////////////////////////
  // Begin with a triangulated graph.  Use a random elimination order so 
  // the starting point of MCMC chain is somewhat random 
  ///////////////////////////////////////////////////////////////////////
  for (crrnt_node = triangulate_nodes.begin(),
       end_node   = triangulate_nodes.end();
       crrnt_node != end_node;
       ++crrnt_node) {
    order.push_back( &(*crrnt_node) ); 
  }
  random_shuffle( order.begin(), order.end() );
  fillInComputation( order, fill_in );

  ////////////////////////////////////////////////////////////////////////
  // Run MCMC chain (with no temperature parameter all moves are accepted) 
  ////////////////////////////////////////////////////////////////////////
  if ((fill_in.size() > 0) ||
      (missing.size() > 0)) {
    (void) edgeAnnealChain(
      triangulate_nodes,
      fill_in,
      missing, 
      chain_length,
      DBL_MAX,
      dummy_triangulation,
      best_graph_weight,
      best_this_weight,
      weight_sum,         
      weight_sqr_sum,
      jtWeight,
      nodesRootMustContain,
      false );
  }

  ///////////////////////////////////////////////////////////////////
  // Get maximal cliques of triangulation 
  ///////////////////////////////////////////////////////////////////
  maximumCardinalitySearch( triangulate_nodes, cliques, order, false );
  assert( testZeroFillIn(order) ); 
  listVectorCliquetoVectorSetClique( cliques, best_cliques );
  fillAccordingToCliques( best_cliques );
}


/*-
 *-----------------------------------------------------------------------
 * edgeAnnealChain 
 *    Generate a chain of triangulations that have one edge different between
 *    them.  This is for use in MCMC methods that can generate any
 *    triangulation.  Edge based simulated annealing is not currently
 *    implemented. 
 *
 * Preconditions:
 *
 * Postconditions:
 *
 * Side Effects:
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
unsigned
BoundaryTriangulate::
edgeAnnealChain(
  vector<triangulateNode>&          nodes,
  vector<edge>&                     fill_in,
  vector<edge>&                     missing,
  const unsigned                    iterations,
  const double                      temperature,
  vector<triangulateNghbrPairType>& best_triangulation,
  double&                           best_graph_weight,
  double&                           best_this_weight,
  double&                           weight_sum,         
  double&                           weight_sqr_sum,
  const bool                        jtWeight,
  const set<RV*>&                   nodesRootMustContain,
  const bool                        use_temperature 
  )
{
  vector<MaxClique>               rv_cliques;
  list<vector<triangulateNode*> > list_cliques;

  edge change_edge;
  bool edge_added = false;

  triangulateNeighborType::iterator found_nghbr;
  vector<edge>::iterator            found_edge; 

  RAND             rndm_nmbr(0);
  double           crrnt_graph_weight;
  double           prvs_graph_weight;
  double           tmprtr_penalty;
  double           crrnt_tmprtr;
  unsigned         moves_accepted;
  unsigned         i;
  bool             accepted;

  crrnt_graph_weight = DBL_MAX;
  prvs_graph_weight  = DBL_MAX;
  best_this_weight   = DBL_MAX;
  moves_accepted     = 0;

  ///////////////////////////////////////////////////////////////////////
  // Permute nodes 'iterations' times  
  ///////////////////////////////////////////////////////////////////////
  for(i=0; i<iterations; ++i) {

    edge_added = changeOneEdge( nodes, fill_in, missing, change_edge, 
      list_cliques );

    listVectorCliquetoVectorSetClique( list_cliques, rv_cliques );
    crrnt_graph_weight = graphWeight(rv_cliques, jtWeight, nodesRootMustContain);
 
    ////////////////////////////////////////////////////////////////
    // Check if it is the best ordering so far 
    ////////////////////////////////////////////////////////////////
    if (crrnt_graph_weight < best_graph_weight) {
      best_graph_weight = crrnt_graph_weight;
      saveCurrentNeighbors( nodes, best_triangulation );  
    } 

    if (crrnt_graph_weight < best_this_weight) {
      best_this_weight = crrnt_graph_weight; 
    }

    ////////////////////////////////////////////////////////////////
    // If new weight is better always accept it.  If weight is 
    //  worse, accept it with a probability calculated from the 
    //  temperature. 
    ////////////////////////////////////////////////////////////////
    if (edge_added == true) {
      crrnt_tmprtr = temperature*1;
    }
    else {
      crrnt_tmprtr = temperature;
    }

    accepted = true;
    if ( ((crrnt_graph_weight-prvs_graph_weight) > 1e-8) && 
          (use_temperature == true) ) {
      tmprtr_penalty = crrnt_tmprtr * log(rndm_nmbr.drand48());

      if ( crrnt_graph_weight > (prvs_graph_weight-tmprtr_penalty)) {
        accepted = false;
      }
    }

    ////////////////////////////////////////////////////////////////
    // Update depending on acceptance/rejection 
    ////////////////////////////////////////////////////////////////
    if (accepted == true) {
      prvs_graph_weight = crrnt_graph_weight;
      weight_sum += prvs_graph_weight;
      weight_sqr_sum += prvs_graph_weight*prvs_graph_weight;
      ++moves_accepted;

      if (edge_added == true) {
        fill_in.push_back( change_edge );
        found_edge = find( missing.begin(), missing.end(), change_edge );
        missing.erase( found_edge );
      }
      else {
        missing.push_back( change_edge );
           found_edge = find( fill_in.begin(), fill_in.end(), change_edge );   
        fill_in.erase( found_edge );
      }
    }
    else {
      if (edge_added == true) {
        found_nghbr = find( change_edge.first()->neighbors.begin(),
        change_edge.first()->neighbors.end(), change_edge.second() );
        assert(found_nghbr != change_edge.first()->neighbors.end());  
        change_edge.first()->neighbors.erase( found_nghbr );

        found_nghbr = find( change_edge.second()->neighbors.begin(),
        change_edge.second()->neighbors.end(), change_edge.first() );
        assert(found_nghbr != change_edge.second()->neighbors.end());  
        change_edge.second()->neighbors.erase( found_nghbr );
      }
      else {
        change_edge.first()->neighbors.push_back( change_edge.second() ); 
        change_edge.second()->neighbors.push_back( change_edge.first() ); 
      }
    }

  } // End for iterations

  return(moves_accepted);
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::changeOneEdge
 *   Takes an input triangulation and creates a triangulations differs by one
 *   edge.  This is done by randomly changing the status of a single edge, and
 *   rejecting the change if the new graph is not triangulated.
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set.  Input graph must be triangulated or this will never 
 *   halt.  
 * 
 * Postconditions:
 *   The edges in the graph are set to this triangulation.  (cliques are not
 *   found)
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
changeOneEdge(
  vector<triangulateNode>& nodes,
  vector<edge>&            fill_in,
  vector<edge>&            missing,
  edge&                    change_edge,
  list<vector<triangulateNode*> >& cliques
  )
{
  triangulateNeighborType::iterator found_nghbr;
  vector<edge>::iterator            found_edge; 
  vector<triangulateNode*>          dummy_order;
  RAND     rndm_nmbr(0);
  unsigned index;
  unsigned total_fill_in;
  bool     chordal;
  bool     edge_added = false;

  total_fill_in = fill_in.size() + missing.size();

  if (total_fill_in > 0) {
    do  { 

      ////////////////////////////////////////////////////////////////
      // Pick an edge and swap its status 
      ////////////////////////////////////////////////////////////////
      index = rndm_nmbr.uniform( total_fill_in-1 );

      if (index < fill_in.size()) { 
        
        change_edge = fill_in[index];

        found_nghbr = find( change_edge.first()->neighbors.begin(),
          change_edge.first()->neighbors.end(), change_edge.second() );
        assert(found_nghbr != change_edge.first()->neighbors.end());  
        change_edge.first()->neighbors.erase( found_nghbr );

        found_nghbr = find( change_edge.second()->neighbors.begin(),
          change_edge.second()->neighbors.end(), change_edge.first() );
        assert(found_nghbr != change_edge.second()->neighbors.end());  
        change_edge.second()->neighbors.erase( found_nghbr );
        edge_added = false;
      }
      else {
        
        index -= fill_in.size(); 
        change_edge = missing[index];
        
        change_edge.first()->neighbors.push_back( change_edge.second() ); 
        change_edge.second()->neighbors.push_back( change_edge.first() ); 
        edge_added = true;
      } 

      ////////////////////////////////////////////////////////////////
      // Test chordality 
      ////////////////////////////////////////////////////////////////
      maximumCardinalitySearch( nodes, cliques, dummy_order, false );
      chordal = testZeroFillIn( dummy_order ); 

      if (! chordal) {
        if (edge_added == true) {

          found_nghbr = find( change_edge.first()->neighbors.begin(),
            change_edge.first()->neighbors.end(), change_edge.second() );
          assert(found_nghbr != change_edge.first()->neighbors.end());  
          change_edge.first()->neighbors.erase( found_nghbr );

          found_nghbr = find( change_edge.second()->neighbors.begin(),
            change_edge.second()->neighbors.end(), change_edge.first() );
          assert(found_nghbr != change_edge.second()->neighbors.end());  
          change_edge.second()->neighbors.erase( found_nghbr );
        }
        else {
          change_edge.first()->neighbors.push_back( change_edge.second() ); 
          change_edge.second()->neighbors.push_back( change_edge.first() ); 
        }
      }
    } while (! chordal);
  }

  return(edge_added);
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::calculateMissingEdges
 *   Returns the set of edges missing from the graph
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
calculateMissingEdges(
  vector<triangulateNode>& nodes,
  vector<edge>&            missing
)
{
  vector<triangulateNode>::iterator crrnt_node; 
  vector<triangulateNode>::iterator end_node; 
  vector<triangulateNode>::iterator crrnt_second; 
  vector<triangulateNode>::iterator end_second;
  triangulateNeighborType::iterator found_nghbr; 
  vector<RV*>::iterator found_fill_in; 

  missing.clear();

  for (crrnt_node = nodes.begin(),
       end_node   = nodes.end();
       crrnt_node != end_node;
       ++crrnt_node) {
    for (crrnt_second = crrnt_node,
         ++crrnt_second, 
         end_second = nodes.end();
         crrnt_second != end_second;
         ++crrnt_second) {

      found_nghbr = find( (*crrnt_node).neighbors.begin(), 
        (*crrnt_node).neighbors.end(), &(*crrnt_second) );
      if (found_nghbr == (*crrnt_node).neighbors.end()) {
        missing.push_back( edge( &(*crrnt_node), &(*crrnt_second)) ); 
      } 
    }
  }

}

/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::calculateFillInEdges
 *   Determines the set of edges in the given set of nodes are fill-in (i.e. 
 *   not in the original graph)
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
calculateFillInEdges(
  vector<triangulateNode>& nodes,
  SavedGraph&              orgnl_graph,
  vector<edge>&            fill_in 
)
{
  vector<triangulateNode>::iterator crrnt_node; 
  vector<triangulateNode>::iterator end_node; 
  vector<triangulateNode*>::iterator crrnt_nghbr; 
  vector<triangulateNode*>::iterator end_nghbr;
  triangulateNeighborType::iterator found_nghbr; 
  set<RV*>::iterator found_fill_in; 
  SavedGraph::iterator crrnt_saved;
  vector<edge>::iterator found_edge; 

  fill_in.clear();

  for (crrnt_node = nodes.begin(),
       end_node   = nodes.end();
       crrnt_node != end_node;
       ++crrnt_node) {

    crrnt_saved = orgnl_graph.begin();
    while ((*crrnt_saved).first != (*crrnt_node).randomVariable) {
      ++crrnt_saved;
      assert(crrnt_saved != orgnl_graph.end());
    }

    for (crrnt_nghbr = (*crrnt_node).neighbors.begin(),
         end_nghbr = (*crrnt_node).neighbors.end();
         crrnt_nghbr != end_nghbr;
         ++crrnt_nghbr ) {

      if (*((*crrnt_saved).second.begin()) != NULL)
      {
        found_fill_in = find( (*crrnt_saved).second.begin(), 
          (*crrnt_saved).second.end(), (*crrnt_nghbr)->randomVariable );

        if (found_fill_in == (*crrnt_saved).second.end()) {
          found_edge = find( fill_in.begin(), fill_in.end(), 
            edge( &(*crrnt_node), *crrnt_nghbr) ); 
          if (found_edge == fill_in.end()) {
            fill_in.push_back( edge( &(*crrnt_node), *crrnt_nghbr) ); 
          }
        }
      }
    }
  } 
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNode::triangulateNode (default constructor) 
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
BoundaryTriangulate::
triangulateNode::
triangulateNode(
  void 
  )
  : randomVariable( NULL ), 
    nodeList( NULL ),
    cardinality( 0 ),
    position( 0 ),
    eliminated( false ),
    marked( false ),
    previousNode( NULL ),
    nextNode( NULL )
{
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNode::triangulateNode (constructor) 
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
BoundaryTriangulate::
triangulateNode::
triangulateNode(
  RV* random_variable 
  )
  : randomVariable( random_variable ), 
    nodeList( NULL ),
    cardinality( 0 ),
    position( 0 ),
    eliminated( false ),
    marked( false ),
    previousNode( NULL ),
    nextNode( NULL )
{
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNodeList::triangulateNodeList (constructor)
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
BoundaryTriangulate::
triangulateNodeList::
triangulateNodeList()
  : last( NULL ),
    list_length( 0 )
{
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNodeList::push_back
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateNodeList::
push_back(
  triangulateNode* node 
  )
{
  node->previousNode = last; 
  node->nextNode     = NULL;
  node->nodeList     = this;

  if (last != NULL) { 
    last->nextNode = node;
  }

  last = node;
  list_length++;
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNodeList::pop_back
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   The last node is removed from the list 
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   A pointer to the (former) last node is returned  
 *-----------------------------------------------------------------------
 */
BoundaryTriangulate::triangulateNode*
BoundaryTriangulate::
triangulateNodeList::
pop_back()
{
  triangulateNode* deleted; 

  if (last->previousNode != NULL) {
    last->previousNode->nextNode = NULL; 
  }

  deleted = last;
  last    = last->previousNode;

  deleted->nextNode     = NULL;
  deleted->previousNode = NULL;
  deleted->nodeList     = NULL;
  
  list_length--;

  return(deleted);
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNodeList::erase
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   The given node is removed from the list.  The memory for the node 
 *   is not-deallocated.
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateNodeList::
erase(
  triangulateNode* node 
  )
{
  assert(node->nodeList == this); 

  if (node == last) {
    last = last->previousNode;
  }

  if (node->previousNode != NULL) {
    node->previousNode->nextNode = node->nextNode;
  }

  if (node->nextNode != NULL) {
    node->nextNode->previousNode = node->previousNode;
  }

  node->nextNode     = NULL;
  node->previousNode = NULL;
  node->nodeList     = NULL;
  
  list_length--;
}


/*-
 *-----------------------------------------------------------------------
 * triangulateNode::operator[]
 *
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   A pointer to the i'th node in the list.  
 *
 * Complexity:
 *   This is a O(N) operation. 
 *-----------------------------------------------------------------------
 */
BoundaryTriangulate::triangulateNode* 
BoundaryTriangulate::
triangulateNodeList::
operator[] (
  unsigned i 
  )
{
  triangulateNode* node; 
  unsigned count; 

  assert(i < list_length);

  node = last;
  for( count=list_length-1; count>i; count-- ) {
    assert(node != NULL);
    node = node->previousNode;
  } 

  return(node);
}


/*-
 *-----------------------------------------------------------------------
 * fillTriangulateNodeStructures
 *
 * Preconditions:
 *   none 
 *
 * Postconditions:
 *   The graph structure given by a set of RV*'s is copied 
 *   into a set of triangulateNode's.  
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   none
 *
 * Complexity:
 *   O(N+E*log(N)) where E is the number of edges and N is the number 
 *   of nodes.
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
fillTriangulateNodeStructures( 
  const set<RV*>& orgnl_nodes,
  vector<triangulateNode>&    new_nodes 
  )
{
  map<RV*, triangulateNode*> rv_to_mcs; 
  set<RV*>::iterator         crrnt_node;
  set<RV*>::iterator         end_node; 
  vector<triangulateNode>::iterator      crrnt_triangulate; 
  vector<triangulateNode>::iterator      end_triangulate; 
  triangulateNode                        new_node;

  ////////////////////////////////////////////////////////////////////////
  // Create a triangulateNode instance for each random variable 
  ////////////////////////////////////////////////////////////////////////
  new_nodes.clear();

  for (crrnt_node = orgnl_nodes.begin(),  
       end_node   = orgnl_nodes.end();  
       crrnt_node != end_node;
       ++crrnt_node) { 

    new_node.randomVariable = *crrnt_node;
    new_nodes.push_back( new_node );
  }

  ////////////////////////////////////////////////////////////////////////
  // Create map from the randomVariable* to its triangulateNode 
  ////////////////////////////////////////////////////////////////////////

  for (crrnt_triangulate = new_nodes.begin(),  
       end_triangulate   = new_nodes.end();  
       crrnt_triangulate != end_triangulate;
       ++crrnt_triangulate) {
 
    rv_to_mcs[(*crrnt_triangulate).randomVariable] = &(*crrnt_triangulate);
  }

  ////////////////////////////////////////////////////////////////////////
  // Create neighbor sets composed of triangulateNode's which match the 
  // original sets of RV*'s    
  ////////////////////////////////////////////////////////////////////////
  set<RV*>::iterator crrnt_nghbr;
  set<RV*>::iterator end_nghbr; 

  for (crrnt_node = orgnl_nodes.begin(),  
       end_node   = orgnl_nodes.end();  
       crrnt_node != end_node;
       ++crrnt_node) {

    for (crrnt_nghbr = (*crrnt_node)->neighbors.begin(),  
         end_nghbr   = (*crrnt_node)->neighbors.end();  
         crrnt_nghbr != end_nghbr;
         ++crrnt_nghbr) {

      rv_to_mcs[*crrnt_node]->neighbors.push_back( rv_to_mcs[*crrnt_nghbr] ); 
    }  
  }
}


/*-
 *-----------------------------------------------------------------------
 * maximumCardinalitySearch
 *   
 *   Calculates a perfect ordering on the graph, if it exists.  If the 
 *   order is perfect, the maximal cliques of the graph are determined.
 *   The procedure can be used as a first step in an O(N+E) chordality 
 *   test.  The procedure can also be used as a heuristic search for an 
 *   optimal elimination order.  If the given graph is not triangulated 
 *   the cliques correspond to the maximal cliques that will occur if 
 *   elimination is run on the graph using the given order.  
 *
 *   The function calculates the order from back to front.  At each step 
 *   it chooses the node with the largest number of previously ordered 
 *   neighbors. 
 *
 *   If randomize_order = true, when there is a tie for the node with the 
 *   largest number of previously ordered neighbors the tie is broken 
 *   randomly.  In this case, the function runs in O(N^2). 
 *
 *   If randomize_order = false, ties are broken in a deterministic
 *   manner and the function runs in O(N+E).  
 *
 *   Maximum cardinality search was originally described in:
 *     R. E. Tarjan and M. Yannakakis.  "Simple linear time algorithm to 
 *     test chordality of graphs, test acyclicity of hypergraphs, and 
 *     selectively reduce acyclic hypergraphs."  SIAM J. Comput., 
 *     13:566--579, 1984.  
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   order is cleared and replaced with an order based on the give nodes. 
 *  
 *   cliques is cleared and replaced with a new list based on the given 
 *   nodes.   
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   none 
 *
 *-----------------------------------------------------------------------
 */
void 
BoundaryTriangulate::
maximumCardinalitySearch( 
  vector<triangulateNode>&         nodes, 
  list<vector<triangulateNode*> >& cliques,
  vector<triangulateNode*>&        order,
  bool                             randomize_order 
  )
{
  vector<triangulateNodeList> card_set;

  vector<triangulateNode*> empty_clique; 
  list<vector<triangulateNode*> >::iterator prvs_clique;
  list<vector<triangulateNode*> >::iterator crrnt_clique;
  list<vector<triangulateNode*> >::iterator tmp_clique;

  vector<triangulateNode>::iterator  crrnt_node; 
  vector<triangulateNode>::iterator  end_node; 
  vector<triangulateNode*>::iterator crrnt_nghbr; 
  vector<triangulateNode*>::iterator end_nghbr; 

  int              max_cardinality;    // largest cardinality found so far
  int              index;              // index of selected variable 
  triangulateNode* elmnt_node;         // node selected for elimination
  unsigned         nmbr_nodes_ordered; // count of number of nodes ordered
  RAND             rndm_nmbr(0);       // random number class

  ////////////////////////////////////////////////////////////////////
  // Put all nodes in cardinality set 0 
  ////////////////////////////////////////////////////////////////////
  card_set.resize( nodes.size()+1 );

  for (crrnt_node = nodes.begin(), 
       end_node = nodes.end(); 
       crrnt_node != nodes.end();
       ++crrnt_node ) {

    (*crrnt_node).cardinality = 0;
    (*crrnt_node).eliminated  = false;
    card_set[0].push_back( &(*crrnt_node) ); 
  }

  max_cardinality = 0;
  
  ////////////////////////////////////////////////////////////////////
  // Set up the current clique and previous clique.
  ////////////////////////////////////////////////////////////////////
  cliques.clear();
  empty_clique.reserve( nodes.size() );
 
  cliques.push_back(empty_clique);
  prvs_clique = cliques.end();
  --prvs_clique;
  cliques.push_back(empty_clique);
  crrnt_clique = cliques.end();
  --crrnt_clique;

  ////////////////////////////////////////////////////////////////////
  // Iterate through all of the nodes 
  ////////////////////////////////////////////////////////////////////
  order.clear();

  for ( nmbr_nodes_ordered = 0;
        nmbr_nodes_ordered < nodes.size();
        nmbr_nodes_ordered++ ) {

    ////////////////////////////////////////////////////////////////////
    // Find unordered node with the largest number of ordered neighbors
    ////////////////////////////////////////////////////////////////////
    if (! randomize_order) {

      elmnt_node = card_set[max_cardinality].pop_back();
    }
    else { 
      ////////////////////////////////////////////////////////////////
      // Choose randomly among all nodes tied for maximum cardinality
      ////////////////////////////////////////////////////////////////
      index = rndm_nmbr.uniform( card_set[max_cardinality].size() - 1 );

      ////////////////////////////////////////////////////////////////
      // Get the pointer referred to by the index, be warned that 
      //  there is an O(N) operation hidden in the second [] 
      ////////////////////////////////////////////////////////////////
      elmnt_node = card_set[max_cardinality][index];  

      card_set[max_cardinality].erase(elmnt_node);
    }

    assert(elmnt_node != NULL);
    order.push_back(elmnt_node); 
    elmnt_node->eliminated = true;

    //////////////////////////////////////////////////////////////////
    // a) Move the node's ordered neighbors into a higher cardinality 
    //    set 
    // b) Remember the node's unordered neighbors as a potential max
    //    clique 
    //////////////////////////////////////////////////////////////////
    (*crrnt_clique).push_back( elmnt_node );
   
    for (crrnt_nghbr = elmnt_node->neighbors.begin(), 
         end_nghbr   = elmnt_node->neighbors.end(); 
         crrnt_nghbr != end_nghbr;
         ++crrnt_nghbr) {

      if ((*crrnt_nghbr)->eliminated == false) {

        card_set[(*crrnt_nghbr)->cardinality].erase( *crrnt_nghbr );
        (*crrnt_nghbr)->cardinality++; 
        card_set[(*crrnt_nghbr)->cardinality].push_back( *crrnt_nghbr );

      }
      else {

        (*crrnt_clique).push_back( *crrnt_nghbr );   
      }
    }

    ////////////////////////////////////////////////////////////////////
    // If the previous node had more or the same number of unordered 
    //   neighbors prvs_clique is a maximal clique.  Set prvs_clique as
    //   the crrnt_clique and start a new current clique.
    ////////////////////////////////////////////////////////////////////
    if ((*prvs_clique).size() >= (*crrnt_clique).size()) {

      prvs_clique = crrnt_clique;
      cliques.push_back(empty_clique);
      crrnt_clique = cliques.end();
      --crrnt_clique; 
    }
    ////////////////////////////////////////////////////////////////////
    // Maximumal clique is not detected, so swap prvs_clique and  
    // crrnt_clique and clear the new crrnt_clique.  This sets the 
    // previous clique to be the current clique and clears next 
    // iteration's current clique without deallocating or allocating any
    // memory. 
    ////////////////////////////////////////////////////////////////////
    else {
      cliques.splice(prvs_clique, cliques, crrnt_clique );

      tmp_clique   = prvs_clique;
      prvs_clique  = crrnt_clique;
      crrnt_clique = tmp_clique;
      (*crrnt_clique).clear();
    }

    //////////////////////////////////////////////////////////////////
    // Set the maximum cardinality  
    //////////////////////////////////////////////////////////////////
    ++max_cardinality;

    while( (max_cardinality>=0) && (card_set[max_cardinality].size()==0) ) {
      --max_cardinality;
    }
  }

  //////////////////////////////////////////////////////////////////
  // Remove the current clique (which will just be the last node 
  // and not a maximal clique)
  //////////////////////////////////////////////////////////////////
  cliques.erase( crrnt_clique ); 

  //////////////////////////////////////////////////////////////////
  // Put the order in forward elimination order 
  //////////////////////////////////////////////////////////////////
  reverse( order.begin(), order.end() );
}  


/*-
 *-----------------------------------------------------------------------
 * fillInComputation
 *  
 *   Triangulates a graph according to a given elimination order in 
 *   O(N+E') time, where N is the number of nodes and E' is the number 
 *   of original graph edges plus the number of fill in edges. 
 *
 *   This algorithm is given in:
 *     R. E. Tarjan and M. Yannakakis.  "Simple linear time algorithm to 
 *     test chordality of graphs, test acyclicity of hypergraphs, and 
 *     selectively reduce acyclic hypergraphs."  SIAM J. Comput., 
 *     13:566--579, 1984.  
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The graph is triangulated according to the elimination order 
 *   ordered_nodes. 
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none 
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
fillInComputation( 
  vector<triangulateNode*>& ordered_nodes,
  vector<edge>&             fill_in,
  bool                      calculate_fill_in
  )
{
  const unsigned invalid_index = ~0;

  vector<triangulateNode*> follower; 
  vector<unsigned>         index;
  vector<edge>             edges;  
 
  vector<triangulateNode*>::iterator crrnt_node;
  vector<triangulateNode*>::iterator end_node;
  triangulateNeighborType::iterator  crrnt_nghbr;
  triangulateNeighborType::iterator  end_nghbr;
  vector<edge>::iterator             crrnt_edge;  
  vector<edge>::iterator             end_edge;  
  unsigned i;
 
  triangulateNode* new_nghbr;  

  ////////////////////////////////////////////////////////////////////
  // Set up follower and index arrays 
  ////////////////////////////////////////////////////////////////////
  follower.resize(ordered_nodes.size());
  index.resize(ordered_nodes.size(), invalid_index);

  ////////////////////////////////////////////////////////////////////
  // Record each node's position in the order into the triangulateNode 
  // structure so that positions can be determined from the pointers 
  ////////////////////////////////////////////////////////////////////
  for (unsigned j = 0; j<ordered_nodes.size(); j++) {

    ordered_nodes[j]->position = j; 
  }

  ////////////////////////////////////////////////////////////////////
  // Iterate through all nodes 
  ////////////////////////////////////////////////////////////////////
  for (crrnt_node = ordered_nodes.begin(), 
       end_node   = ordered_nodes.end(),
       i = 0;
       crrnt_node != end_node;
       ++i, ++crrnt_node ) {

    follower[i] = *crrnt_node; 
    index[i]    = i;

    ////////////////////////////////////////////////////////////////////
    // Find edges {crrnt_node, new_nghbr} with (crrnt_node<new_nghbr) 
    // such that there is a vertex crrnt_nghbr with with 
    // {crrnt_nghbr, new_nghbr} is a graph edge and 
    // follower^i(crrnt_nghbr)=crrnt_node  
    ////////////////////////////////////////////////////////////////////
    for (crrnt_nghbr = (*crrnt_node)->neighbors.begin(), 
         end_nghbr   = (*crrnt_node)->neighbors.end();
         crrnt_nghbr != end_nghbr; 
         ++crrnt_nghbr ) {

      if ((*crrnt_nghbr)->position < i) {

        new_nghbr = *crrnt_nghbr; 
        assert( index[new_nghbr->position] != invalid_index );

        while ( index[new_nghbr->position] < i ) { 

          index[new_nghbr->position] = i;
          edges.push_back( edge(*crrnt_node, new_nghbr) );
          new_nghbr = follower[new_nghbr->position]; 
        }

        if (follower[new_nghbr->position] == new_nghbr) {

          follower[new_nghbr->position] = *crrnt_node;
        } 
      } 
    } 
  }   

  ////////////////////////////////////////////////////////////////////
  // Calculate the fil-in 
  ////////////////////////////////////////////////////////////////////
  if (calculate_fill_in)
  {
    triangulateNeighborType::iterator found_n;

    fill_in.clear();
    for (crrnt_edge = edges.begin(), 
         end_edge   = edges.end();
         crrnt_edge != end_edge;
         ++crrnt_edge ) {

      found_n = find( (*crrnt_edge).first()->neighbors.begin(),
                      (*crrnt_edge).first()->neighbors.end(),
                      (*crrnt_edge).second() );
      if ( found_n == (*crrnt_edge).first()->neighbors.end() ) {
        fill_in.push_back( *crrnt_edge );
      }
    }
  }

  ////////////////////////////////////////////////////////////////////
  // Remove the previous edge sets 
  ////////////////////////////////////////////////////////////////////
 
  for (crrnt_node = ordered_nodes.begin(), 
       end_node   = ordered_nodes.end();
       crrnt_node != end_node;
       ++crrnt_node ) {

    (*crrnt_node)->neighbors.clear();  
  }
 
  ////////////////////////////////////////////////////////////////////
  // Add the new edge sets to the graph 
  ////////////////////////////////////////////////////////////////////
  for (crrnt_edge = edges.begin(), 
       end_edge   = edges.end();
       crrnt_edge != end_edge;
       ++crrnt_edge ) {

    (*crrnt_edge).first()->neighbors.push_back( (*crrnt_edge).second() ); 
    (*crrnt_edge).second()->neighbors.push_back( (*crrnt_edge).first() ); 
  }
}

/*-
 *-----------------------------------------------------------------------
 * fillInComputation
 *  
 *   Triangulates a graph according to a given elimination order in 
 *   O(N+E') time, where N is the number of nodes and E' is the number 
 *   of original graph edges plus the number of fill in edges. 
 *
 *   This algorithm is given in:
 *     R. E. Tarjan and M. Yannakakis.  "Simple linear time algorithm to 
 *     test chordality of graphs, test acyclicity of hypergraphs, and 
 *     selectively reduce acyclic hypergraphs."  SIAM J. Comput., 
 *     13:566--579, 1984.  
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The graph is triangulated according to the elimination order 
 *   ordered_nodes. 
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none 
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
fillInComputation( 
  vector<triangulateNode*>& ordered_nodes
  )
{
  vector<edge> dummy_fill_in;
  fillInComputation(ordered_nodes, dummy_fill_in, false);
}


/*-
 *-----------------------------------------------------------------------
 * testZeroFillIn
 *  
 *  Determines if an elimination order is zero fill in.  This algorithm
 *  runs in O(N+E) time.  Combined with maximum cardinality search this
 *  function provides the second step of a chordality test. 
 *
 *  The algorithm was given in:
 *    R. E. Tarjan and M. Yannakakis.  "Simple linear time algorithm to 
 *    test chordality of graphs, test acyclicity of hypergraphs, and 
 *    selectively reduce acyclic hypergraphs."  SIAM J. Comput., 
 *    13:566--579, 1984.  
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   true if the graph is triangulated   
 *
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
testZeroFillIn( 
  vector<triangulateNode*>& ordered_nodes 
  )
{
  const unsigned invalid_index = ~0;

  vector<triangulateNode*> follower; 
  vector<unsigned> index;
  
  vector<triangulateNode*>::iterator crrnt_node;
  vector<triangulateNode*>::iterator end_node;
  vector<triangulateNode*>::iterator crrnt_nghbr;
  vector<triangulateNode*>::iterator end_nghbr;
  unsigned i;
  bool chordal = true;

  ////////////////////////////////////////////////////////////////////
  // Set up follower and index arrays 
  ////////////////////////////////////////////////////////////////////
  follower.resize(ordered_nodes.size());
  index.resize(ordered_nodes.size(), invalid_index);

  ////////////////////////////////////////////////////////////////////
  // Record each node's position in the order into the triangulateNode 
  // structure so that positions can be determined from the pointers 
  ////////////////////////////////////////////////////////////////////
  for (unsigned j = 0; j<ordered_nodes.size(); j++) {

    ordered_nodes[j]->position = j; 
  }

  ////////////////////////////////////////////////////////////////////
  // Iterate through all nodes 
  ////////////////////////////////////////////////////////////////////
  for (crrnt_node = ordered_nodes.begin(), 
       end_node   = ordered_nodes.end(),
       i = 0;
       (crrnt_node != end_node) && (chordal == true); 
       ++i, ++crrnt_node ) {

    assert( (*crrnt_node)->position == i ); 
    follower[i] = *crrnt_node; 
    index[i]    = i;

    ////////////////////////////////////////////////////////////////////
    // Calculate the followers and indexes of ordered neighbors 
    ////////////////////////////////////////////////////////////////////
    for (crrnt_nghbr = (*crrnt_node)->neighbors.begin(), 
         end_nghbr   = (*crrnt_node)->neighbors.end();
         crrnt_nghbr != end_nghbr; 
         ++crrnt_nghbr ) {

      assert( (*crrnt_nghbr)->position != invalid_index ); 

      if ((*crrnt_nghbr)->position < i) {

        index[(*crrnt_nghbr)->position] = i;

        if ( follower[(*crrnt_nghbr)->position] == *crrnt_nghbr ) {
          follower[(*crrnt_nghbr)->position] = *crrnt_node;
        }           
      }
    }

    ////////////////////////////////////////////////////////////////////
    // Graph is not triangulated if any ordered neighbors did not have
    // their indexes set. 
    ////////////////////////////////////////////////////////////////////
    for (crrnt_nghbr = (*crrnt_node)->neighbors.begin(), 
         end_nghbr   = (*crrnt_node)->neighbors.end();
         crrnt_nghbr != end_nghbr; 
         ++crrnt_nghbr ) {

      if ((*crrnt_nghbr)->position < i) { 

        if ( index[(follower[(*crrnt_nghbr)->position])->position] < i ) {
          chordal = false;
        } 
      }
    }
  } 

  return(chordal);
}


/*-
 *-----------------------------------------------------------------------
 * triangulateMaximumCardinalitySearch
 *
 *   The graph is triangulated with an elimination order as determined by 
 *   maximum cardinality search,  The maximal cliques are stored in RIP 
 *   order.  If the input graph is already triangulated it will not be
 *   changed by this function. 
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The graph is triangulated, the maximal cliques of the triangulated
 *   graph are stored in the parameter 'cliques', and the elimination order 
 *   used is stored in the parameter 'order'.
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   none 
 *
 * Complexity:
 *   This procedure runs O((N^2)log(N)), the cost of converting the 
 *   list of vector cliques to the vector of set cliques. 
 * 
 * (Also see triangulateMCSIfNotTriangulated and maximumCardinalitySearch) 
 *-----------------------------------------------------------------------
 */
void 
BoundaryTriangulate::
triangulateMaximumCardinalitySearch( 
  const set<RV*>& nodes,
  vector<MaxClique>&          cliques,
  vector<RV*>&    order
  )
{
  vector<triangulateNode>           triangulate_nodes; 
  list<vector<triangulateNode*> >   list_cliques;
  vector<triangulateNode*>          triangulate_order; 
  vector<triangulateNode*>          dummy_order; 
  vector<triangulateNode>::iterator crrnt_node; 
  vector<triangulateNode>::iterator end_node; 

  fillTriangulateNodeStructures( nodes, triangulate_nodes );

  ////////////////////////////////////////////////////////////////////////// 
  // Find a triangulation using maximum cardinality search 
  ////////////////////////////////////////////////////////////////////////// 
  maximumCardinalitySearch( triangulate_nodes, list_cliques, triangulate_order, 
    true);
  fillInComputation( triangulate_order );

  ////////////////////////////////////////////////////////////////////////// 
  // Now use maximum cardinality search to get the cliques of the 
  // triangulated graph 
  ////////////////////////////////////////////////////////////////////////// 
  maximumCardinalitySearch( triangulate_nodes, list_cliques, dummy_order, 
    false);

  ////////////////////////////////////////////////////////////////////////// 
  // Convert to MaxCliques and triangulate the RV structures 
  ////////////////////////////////////////////////////////////////////////// 
  listVectorCliquetoVectorSetClique( list_cliques, cliques );
  fillAccordingToCliques( cliques );

  ////////////////////////////////////////////////////////////////////////// 
  // Store the order 
  ////////////////////////////////////////////////////////////////////////// 
  for ( crrnt_node = triangulate_nodes.begin(), 
        end_node   = triangulate_nodes.end();  
        crrnt_node != end_node ;
        ++crrnt_node) {
    order.push_back( (*crrnt_node).randomVariable );
  }
}


/*-
 *-----------------------------------------------------------------------
 * chordalityTest 
 *   
 * Runs in O(N+E*log(N)), the cost of converting from random variables to 
 * triangulateNodes
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   none 
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   true if graph is chordal, false if not 
 *
 * Complexity:
 *   O(N+E*log(N)), the cost of converting the RVs to 
 *   triangulateNodes.  
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
chordalityTest( 
  const set<RV*>& nodes
  )
{
  vector<triangulateNode>         triangulate_nodes; 
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        order; 
  bool                            chordal;

  fillTriangulateNodeStructures( nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  chordal = testZeroFillIn( order );

  return(chordal);
}


/*-
 *-----------------------------------------------------------------------
 * getCliques 
 *   
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   If the graph is chordal, the maximal cliques are stored in the 
 *   cliques variable in RIP order.  If the graph is not chordal, the 
 *   cliques are not modified. 
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   true if graph is chordal, false if not 
 *
 * Complexity:
 *   This procedure runs O((N^2)log(N)), the cost of converting the 
 *   list of vector cliques to the vector of set cliques. 
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
getCliques( 
  const set<RV*>& nodes,
  vector<MaxClique>&          cliques
  )
{
  vector<triangulateNode>         triangulate_nodes; 
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        order; 
  bool                            chordal;

  fillTriangulateNodeStructures( nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  chordal = testZeroFillIn( order );

  if (chordal) {
    listVectorCliquetoVectorSetClique( list_cliques, cliques );
  }

  return(chordal);
}


/*-
 *-----------------------------------------------------------------------
 * triangulateMCSIfNotTriangulated
 *
 *   The graph is triangulated with an elimination order as determined by 
 *   maximum cardinality search,  The maximal cliques are stored in RIP 
 *   order.  If the input graph is already triangulated it will not be
 *   changed by this function. 
 *
 *   This procedure performs almost the same function as 
 *   triangulateMaximumCardinalitySearch.  This version has a return 
 *   value telling the user if the original graph was triangulated, does 
 *   not return the elimination order, will be a bit faster if the 
 *   input graph is already triangualted.  This version will always 
 *   return the same triangulation, whereas 
 *   triangulateMaximumCardinalitySearch will randomize if possible. 
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The graph is triangulated, and the maximal cliques of the triangulated
 *   graph are stored in the parameter 'cliques'.
 *
 * Side Effects:
 *   none 
 *
 * Results:
 *   true if input graph is chordal, false if not 
 *
 * Complexity:
 *   This procedure runs O((N^2)log(N)), the cost of converting the 
 *   list of vector cliques to the vector of set cliques. 
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
triangulateMCSIfNotTriangulated( 
  const set<RV*>& nodes,
  vector<MaxClique>&          cliques
  )
{
  vector<triangulateNode>         triangulate_nodes; 
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        order; 
  bool                            chordal;

  fillTriangulateNodeStructures( nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  chordal = testZeroFillIn( order );

  if (!chordal) {
    ////////////////////////////////////////////////////////////////////////// 
    // Triangulate the graph according to the order found using MCS 
    ////////////////////////////////////////////////////////////////////////// 
    fillInComputation( order );

    ////////////////////////////////////////////////////////////////////////// 
    // Use maximum cardinality search to get the cliques of the 
    // triangulated graph 
    ////////////////////////////////////////////////////////////////////////// 
    maximumCardinalitySearch( triangulate_nodes, list_cliques, order, 
      false);
  }

  listVectorCliquetoVectorSetClique( list_cliques, cliques );

  ////////////////////////////////////////////////////////////////////////// 
  // Convert to MaxCliques and triangulate the RV structures 
  ////////////////////////////////////////////////////////////////////////// 
  listVectorCliquetoVectorSetClique( list_cliques, cliques );
  fillAccordingToCliques( cliques );

  return(chordal);
}


/*-
 *-----------------------------------------------------------------------
 * triangulateCompletePartition 
 *   adds eges between every node in the graph
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   Triangulates by adding edge between every node in the graph.  
 *
 * Side Effects:
 *   There are edges between every node in the graph.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateCompletePartition( 
  const set<RV*>& nodes,
  vector<MaxClique>&          cliques
  )
{
  ////////////////////////////////////////////////////////////////////
  // Add an edge between every combination of nodes 
  ////////////////////////////////////////////////////////////////////
  MaxClique::makeComplete(nodes);

  cliques.clear();
  cliques.push_back(MaxClique(nodes));

  return;
}


/*-
 *-----------------------------------------------------------------------
 * triangulateCompleteFrame
 *   Complete all the nodes that are in the same frame plus either the
 *   left or right  interface. 
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   Triangulates by adding edge between every node in the graph.  
 *
 * Side Effects:
 *   There are edges between every node in the graph.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateCompleteFrame( 
  // 
  BoundaryTriangulate::completeFrameType leftRightType, 
  // input: nodes to be triangulated
  const set<RV*>& nodes,
  // use JT weight rather than sum of weight
  const bool jtWeight,
  // nodes that a JT root must contain (ok to be empty).
  const set<RV*>& nodesRootMustContain,
  // triangulation heuristic method
  const string& tri_heur_str,
  // original neighbor structures
  SavedGraph& orgnl_nghbrs,
  // output: resulting max cliques
  vector<MaxClique>& best_cliques,
  // output: string giving resulting method used
  string& best_meth_str,
  // weight to best
  double& best_weight, 
  // Prefix for new best_meth_str 
  string  best_method_prefix
  )
{
  set <RV*>::iterator crrnt_node;
  set <RV*>::iterator end_node;
  set <RV*>::iterator crrnt_nghbr;
  set <RV*>::iterator end_nghbr;
  vector<MaxClique> cliques;
  unsigned min_frame, max_frame, crrnt_frame; 
  set <RV*> crrnt_group;
  double weight;

  ////////////////////////////////////////////////////////////////////
  // Find the frames that exist in this partition
  ////////////////////////////////////////////////////////////////////
  min_frame = (*(nodes.begin()))->timeFrame;
  max_frame = (*(nodes.begin()))->timeFrame;

  for ( crrnt_node = nodes.begin(), end_node = nodes.end();
        crrnt_node != end_node;
        crrnt_node++ )
  {
    if (min_frame > (*crrnt_node)->timeFrame)
    { 
      min_frame = (*crrnt_node)->timeFrame;
    }
    else if (max_frame < (*crrnt_node)->timeFrame)
    { 
      max_frame = (*crrnt_node)->timeFrame;
    }
  }
       
  ////////////////////////////////////////////////////////////////////
  // Iterate through the frames
  ////////////////////////////////////////////////////////////////////
  for (crrnt_frame=min_frame; crrnt_frame<=max_frame; crrnt_frame++)
  {
    crrnt_group.clear();

    ////////////////////////////////////////////////////////////////////
    // Find all nodes in the current frame
    ////////////////////////////////////////////////////////////////////
    for ( crrnt_node = nodes.begin(), end_node = nodes.end();
          crrnt_node != end_node;
          crrnt_node++ )
    {
      if ((*crrnt_node)->timeFrame == crrnt_frame)
      {
        crrnt_group.insert(*crrnt_node);         

        ////////////////////////////////////////////////////////////////////
        // Find any interface nodes connected to the current frame
        ////////////////////////////////////////////////////////////////////
        for ( crrnt_nghbr = (*crrnt_node)->neighbors.begin(), 
              end_nghbr = (*crrnt_node)->neighbors.end(); 
              crrnt_nghbr != end_nghbr;
              crrnt_nghbr++ )
        {
          if (leftRightType == COMPLETE_FRAME_RIGHT) 
          {
            if ((*crrnt_nghbr)->timeFrame < (*crrnt_node)->timeFrame)
            {
              crrnt_group.insert(*crrnt_nghbr);
            }
          }
          else
          {
            if ((*crrnt_nghbr)->timeFrame > (*crrnt_node)->timeFrame) 
            {
              crrnt_group.insert(*crrnt_nghbr);
            }
          }
        } 
      } 
    } 

    ////////////////////////////////////////////////////////////////////
    // Complete the nodes 
    ////////////////////////////////////////////////////////////////////
    MaxClique::makeComplete(crrnt_group);
  }

  ////////////////////////////////////////////////////////////////////
  // Check if the partition is triangulated, if not fix it
  ////////////////////////////////////////////////////////////////////
  vector<triangulateNode>         triangulate_nodes; 
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        order; 
  bool                            chordal;

  fillTriangulateNodeStructures( nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  chordal = testZeroFillIn( order );

  if (chordal)
  {
    listVectorCliquetoVectorSetClique( list_cliques, cliques );
    weight = graphWeight(cliques, jtWeight, nodesRootMustContain);

    if (weight < best_weight) 
    { 
      ////////////////////////////////////////////////////////////////////
      ////////////////////////////////////////////////////////////////////
      //@@@ There is a bug due to the stubbed out = operator in the 
      //   MaxClique 
      //   class the following line which clears the cliques appears to 
      //   work around the bug. @@@
      // TODO: ultimately take this out, but keep in for now until
      // we do code restructuring (1/20/2004).
      best_cliques.clear();
      ////////////////////////////////////////////////////////////////////
      ////////////////////////////////////////////////////////////////////

      best_cliques = cliques;
      best_weight  = weight;
    
      best_meth_str = best_method_prefix;
      infoMsg(IM::Tiny, "   New Best Triangulation Found: %20s %-10f\n", 
        best_meth_str.c_str(), weight);
    }
  }
  else 
  {
    infoMsg(IM::Huge, "Graph not triangulated after complete frame\n");

    string fix_heuristic = tri_heur_str;

    if (fix_heuristic.length()== 0) {
      fix_heuristic = "W";
    }

    if (fix_heuristic[0] == '-') {
      fix_heuristic.erase(0, 1); 
    }

    SavedGraph cmpltd_nghbrs;
    saveCurrentNeighbors(nodes, cmpltd_nghbrs);
    triangulatePartition( nodes, jtWeight, nodesRootMustContain, fix_heuristic, 
      cmpltd_nghbrs, best_cliques, best_meth_str, best_weight, 
      best_method_prefix+"-" ); 
  } 

}


/*-
 *-----------------------------------------------------------------------
 * triangulateFrontier
 *
 *   Triangulate using the Frontier algorithm, a procedure that works
 *   entirely on the directed graph. The Frontier algorithm is defined
 *   in:
 *
 *       "A forward-backward algorithm for inference in Bayesian
 *       networks and an empirical comparison with HMMs", G. Zweig,
 *       Master's Thesis, Dept. of Computer Science, U.C. Berkeley,
 *       May 9th, 1996.
 *
 *   We don't use the Frontier alg. for inference (as it was
 *   originally defined, it was an algorithm to specify the order of
 *   variables to marginalize a big joint probability
 *   distribution). Here, rather, the essentials of the Frontier
 *   algorithm are extracted just to perform a graph triangulation for
 *   us that can be evaluated and compared with the other graph
 *   triangulation heuristics implemented in GMTK.
 *  
 *   Note, that Frontier requires a topological ordering of the nodes.
 *   In this implementation, we first compute a random topological
 *   ordering. This allows this routine to be called many times, each
 *   time it will produce a different triangulation.
 *
 *   Note that the graph doesn't need to be moralized here, as
 *   Frontier does that for us when it selects cliques, but it won't
 *   change things or hurt if it already is.  Also, note that Frontier
 *   sometimes (but rarely) will not triangulate the graph with
 *   respect to the compulsory interface completion edges that have at
 *   this point been added to the graph. If Frontier misses those
 *   edges, then we do a quick MCS pass to fix this up. In practice,
 *   however, this does not happen very often, and also it depends on
 *   the current boundary. If we spent time finding a good boundary
 *   then the cases when Frontier does this are likely to be quite
 *   poor triangulations.
 *   
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   Triangulates by running the Frontier algorithm.
 *
 * Side Effects:
 *   There are edges between every node in each clique in the graph.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::triangulateFrontier(const set<RV*>& nodes,
		      vector<MaxClique>&          cliques
		      )
{
  cliques.clear();
  vector <RV*> sortedNodes;

  GraphicalModel::topologicalSortRandom(nodes,nodes,sortedNodes);
  if (message(High)) {
    infoMsg(High,"Frontier: Sorted Nodes:");
    printRVSet(stdout,sortedNodes);
  }

  set <RV*> frontier;
  set <RV*> cumulativeFrontier;
  for (unsigned i=0;i<sortedNodes.size();i++) {
    RV*nrv = sortedNodes[i];
    if (message(High)) {
      infoMsg(High,"Frontier: Current nodes");
      printRVSet(stdout,frontier);
    }
    
    set <RV*>::iterator it;
    set <RV*>::iterator it_end = frontier.end();
    set <RV*> toRemove;
    for (it=frontier.begin();it!=it_end;it++) {
      RV*rv = (*it);
      bool childrenInFrontier = true;
      // Check if all of rv's children are in the
      // cumulative frontier.
      infoMsg(High,"Frontier: Node %s(%d) has %d children\n",
	      rv->name().c_str(),rv->frame(),rv->allChildren.size());

      for (unsigned c=0;c<rv->allChildren.size();c++) {
	RV* child = rv->allChildren[c];
	// only consider nodes within this partition set.
	if (nodes.find(child) == nodes.end()) {
	  infoMsg(High,"Frontier: Child %d %s(%d) not in nodes\n",
		  c,child->name().c_str(),child->frame());
	  continue;
	}
 	if (cumulativeFrontier.find(child) == cumulativeFrontier.end()) {
	  infoMsg(High,"Frontier: Child %d %s(%d) not in frontier\n",
		  c,child->name().c_str(),child->frame());
	  childrenInFrontier = false;
	  break;
	}
      }
      if (childrenInFrontier)
	toRemove.insert(rv);
    }
    if (toRemove.size() > 0) {
      // we have a clique
      if (message(High)) {
	infoMsg(High,"Frontier: Clique:");
	printRVSet(stdout,frontier);
      }
      MaxClique::makeComplete(frontier);

      // We do not need to remove (reduce) everything in the toRemove
      // set. As another random heuristic, we add the case where we
      // remove only a random subset of the set of nodes that we
      // should remove. This:
      //   1) introduces further randomness into the algorithm (on top of the 
      //      random topological sort we do above),
      //   2) TODO: could be extended to use smarter heuristics other than
      //      taking a random subset.
      //   3) potentially makes the cliques larger (as the next clique that
      //      occurs is going to contain the variables that we have not removed
      //      on this clique.
      //   4) TODO: potentially could add with the ancestral-pair heuristic, so that
      //      we don't remove a node if it is ancestral with respect to some
      //      node in the clique.
      //   5) We make sure we remove at least something, as otherwise
      //      the next clique will include  this one (which is wasteful)


      set <RV*> toRemoveSubset;
      set <RV*>::iterator rem_it;
      set <RV*>::iterator rem_it_end = toRemove.end();
      for (rem_it=toRemove.begin();rem_it!=rem_it_end;rem_it++) {
	// drop entries from remove set with probabiltiy 0.75 for now.
	// TODO: change this to a command line option.
	if (rnd.coin(0.75))
	  toRemoveSubset.insert((*rem_it));
      }

      // Uncomment if we want to remove everything.
      // set <RV*> toRemoveSubset = toRemove;

      set <RV*> res;
      if (message(High)) {
	infoMsg(High,"Frontier: Removing:");
	printRVSet(stdout,toRemoveSubset);
      }

      set_difference(frontier.begin(),frontier.end(),
		     toRemoveSubset.begin(),toRemoveSubset.end(),
		     inserter(res,res.end()));
      frontier = res;
    }
    infoMsg(High,"Frontier: adding node %s(%d)\n",
	    nrv->name().c_str(),nrv->frame());
    // insert new members
    frontier.insert(nrv);
    cumulativeFrontier.insert(nrv);
  }
  // last one is always a clique
  if (message(High)) {
    infoMsg(High,"Frontier: Clique:");
    printRVSet(stdout,frontier);
  }
  MaxClique::makeComplete(frontier);

  // Sometimes, but rarely, frontier might not triangulate the graph
  // since it doesn't know about the extra compulsory edges for the
  // completing the left and right interfaces of the current
  // partition (and it does not know about any other local cliques in the .str file).
  // 
  // In order to make sure, we run a MCS pass adding edges in case
  // Frontier didn't enclose right interface with a clique and it
  // isn't triangulated.  Note that if Frontier indeed triangulated
  // the graph (such that the extra forced completion of the
  // interfaces are included in cliques or the result is
  // triangulated), then this next step is guaranteed not to change
  // the graph at all.
  // TODO: use a better fixup triangulation algorithm other than MCS.
  if (!triangulateMCSIfNotTriangulated(nodes,cliques))
    infoMsg(High,"Frontier: MCS used to fix up frontier triangulation\n");

  return;
}




/*-
 *-----------------------------------------------------------------------
 * listVectorCliquetoVectorSetClique
 *
 * Preconditions:
 *   none
 * 
 * Postconditions:
 *   The vector of MaxCliques matches the list of vectors of 
 *   triangulateNode pointers.  This is useful for converting a graph 
 *   defined by triangulateNode's into a graph defined by RV's
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   none
 *
 * Complexity:
 *   O((N^2)log(N))
 *-----------------------------------------------------------------------
 */
void  
BoundaryTriangulate::
listVectorCliquetoVectorSetClique(
  const list<vector<triangulateNode*> >& lv_cliques,
  vector<MaxClique>&                     vs_cliques
  )
{
  vector<MaxClique>::iterator crrnt_vs_clique;

  list<vector<triangulateNode*> >::const_iterator crrnt_lv_clique; 
  list<vector<triangulateNode*> >::const_iterator end_lv_clique;

  vector<triangulateNode*>::const_iterator crrnt_node;
  vector<triangulateNode*>::const_iterator end_node;
 
  set<RV*> empty_RV_set;
  MaxClique empty_MaxClique( empty_RV_set );

  ////////////////////////////////////////////////////////////////////
  // Iterate through original data structure and construct the new 
  ////////////////////////////////////////////////////////////////////
  vs_cliques.clear();

  for( crrnt_lv_clique = lv_cliques.begin(),  
       end_lv_clique   = lv_cliques.end();  
       crrnt_lv_clique != end_lv_clique;   
       ++crrnt_lv_clique ) {

    vs_cliques.push_back(empty_MaxClique);
    crrnt_vs_clique = vs_cliques.end();
    --crrnt_vs_clique; 
     
    for( crrnt_node = (*crrnt_lv_clique).begin(),  
         end_node   = (*crrnt_lv_clique).end();  
         crrnt_node != end_node;   
         ++crrnt_node ) {

      (*crrnt_vs_clique).nodes.insert( (*crrnt_node)->randomVariable );   
    }
  }

}


/*-
 *-----------------------------------------------------------------------
 * triangulateExhaustiveSearch 
 *   Tries every possible combination of edges to determine find a 
 *   minimum weight triangulation.
 * 
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   nodes in the set. 
 *
 * Postconditions:
 *   The minimum weight triangulation is stored in best_cliques.  Graph 
 *   is triangulated.
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.  
 *
 * Results:
 *      none
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateExhaustiveSearch( 
  const set<RV*>&  nodes,
  const bool                   jtWeight,
  const set<RV*>&  nodesRootMustContain,
  const SavedGraph&            orgnl_nghbrs,
  vector<MaxClique>&           best_cliques   
  )
{
  // define local constants used in this routine.
  const unsigned NONE = 0;
  const unsigned FILL_IN = 1;
  const unsigned EDGE = 2; 
  const unsigned UNUSED = 3;

  vector<vector<unsigned> > adjacency;  
  vector<MaxClique>         vector_cliques;

  unsigned nmbr_empty, nmbr_nodes;
  unsigned i, j;
  unsigned row, col;
  bool     triangulation_found = false;

  vector<triangulateNode>            triangulate_nodes; 

  list<vector<triangulateNode*> > cliques;
  list<vector<triangulateNode*> > best_list_cliques;

  list<vector<triangulateNode*> >::iterator crrnt_clique;
  list<vector<triangulateNode*> >::iterator end_clique;

  vector<triangulateNode*>           order; 
  vector<triangulateNode*>::iterator crrnt_node;
  vector<triangulateNode*>::iterator end_node;
  vector<triangulateNode*>::iterator nghbr; 

  ////////////////////////////////////////////////////////////////////
  // Reserve space in the containers 
  ////////////////////////////////////////////////////////////////////
  vector_cliques.reserve( nodes.size() );
  triangulate_nodes.reserve( nodes.size() ); 
  order.reserve( nodes.size() ); 

  ////////////////////////////////////////////////////////////////////
  // Create triagulateNode object from the RV set 
  ////////////////////////////////////////////////////////////////////
  fillTriangulateNodeStructures( nodes, triangulate_nodes );

  ////////////////////////////////////////////////////////////////////
  // Build the adjacency matrix 
  ////////////////////////////////////////////////////////////////////

  nmbr_empty = 0;
  nmbr_nodes = nodes.size();
  adjacency.resize( nmbr_nodes );

  for( i=0; i<nmbr_nodes; i++) {

    adjacency[i].resize( nmbr_nodes, UNUSED );

    for( j=i+1; j<nmbr_nodes; j++) {
  
      nghbr = find( triangulate_nodes[i].neighbors.begin(), 
        triangulate_nodes[i].neighbors.end(), &triangulate_nodes[j] );   
      if (nghbr == triangulate_nodes[i].neighbors.end()) {
         adjacency[i][j] = NONE;
         nmbr_empty++; 
      }
      else {
        adjacency[i][j] = EDGE;
      } 
    }
  } 
  
  ////////////////////////////////////////////////////////////////////
  // Display node key and initial matrix 
  ////////////////////////////////////////////////////////////////////
  infoMsg(IM::Moderate, "----------\n"); 
        
  for( i=0; i<nmbr_nodes; i++) {
    infoMsg(IM::Moderate, "[%d] %s(%d)\n", i, 
      triangulate_nodes[i].randomVariable->name().c_str(), 
      triangulate_nodes[i].randomVariable->frame() );
  } 
  infoMsg(IM::Moderate, "\n");

  infoMsg(IM::Moderate, "   ");
  for( i=0; i<nmbr_nodes; i++) {
    infoMsg(IM::Moderate, "[%2d]", i);
  }
  infoMsg(IM::Moderate, "\n");

  for( i=0; i<nmbr_nodes; i++) {
    infoMsg(IM::Moderate, "[%2d]", i );
    for( j=0; j<nmbr_nodes; j++) {
      infoMsg(IM::Moderate," %2d ", adjacency[i][j]);
    }
    infoMsg(IM::Moderate, "\n");
  }  

  ////////////////////////////////////////////////////////////////////
  // Begin Search 
  ////////////////////////////////////////////////////////////////////
  double best_weight = DBL_MAX; 
  double weight; 
  bool done = false;
  bool chordal;
  double nmbr_cmbntns;
  unsigned crrnt_trial;

  if (nmbr_empty < 1024) {
    nmbr_cmbntns = pow(2.0, (double)nmbr_empty);
  }
  else {
    nmbr_cmbntns = -1; 
  }
 
  crrnt_trial  = 0;

  if (nmbr_empty < 1024) {
    infoMsg(IM::Tiny, "%d nodes, %d missing edges, %0e combinations\n", 
      nmbr_nodes , nmbr_empty, nmbr_cmbntns );
  } 
  else {
    infoMsg(IM::Tiny, "%d nodes, %d missing edges, 2^%d combinations\n", 
      nmbr_nodes , nmbr_empty, nmbr_empty );
  } 

  while (!done) {

    crrnt_trial++;
    if ((crrnt_trial % 0x10000) == 0) {

      if (nmbr_empty < 1024) { 
        infoMsg(IM::Low, "%e of %0e\n", (double)crrnt_trial, nmbr_cmbntns );
      }
      else {
        infoMsg(IM::Low, "%e of 2^%d\n", (double)crrnt_trial, nmbr_empty);
      }
    }
 
    //////////////////////////////////////////////////////////////////
    // Test current configuration  
    //////////////////////////////////////////////////////////////////
    maximumCardinalitySearch( triangulate_nodes, cliques, order, false);
    chordal = testZeroFillIn( order );

    if (chordal) {

      triangulation_found = true;

      listVectorCliquetoVectorSetClique( cliques, vector_cliques );
      weight = graphWeight(vector_cliques,jtWeight,nodesRootMustContain);

      if (weight < best_weight) {
        infoMsg(IM::Low, "----- New Best: %f -----\n", weight); 

        infoMsg(IM::Moderate, "    ");
        for( i=0; i<nmbr_nodes; i++) {
          infoMsg(IM::Moderate, "[%2d]", i );
        } 
        infoMsg(IM::Moderate, "\n"); 

        for( i=0; i<nmbr_nodes; i++) {
          infoMsg(IM::Moderate, "[%2d]", i );
          for( j=0; j<nmbr_nodes; j++) {
            infoMsg(IM::Moderate, " %2d ", adjacency[i][j]);
          } 
          infoMsg(IM::Moderate, "\n"); 
        } 

        for( crrnt_clique = cliques.begin(), 
             end_clique   = cliques.end();
             crrnt_clique != end_clique; 
             ++crrnt_clique ) {

          infoMsg(IM::Moderate, "--- Clique ---\n"); 
          for( crrnt_node = (*crrnt_clique).begin(), 
	       end_node   = (*crrnt_clique).end(); 
               crrnt_node != end_node;
	       crrnt_node++ ) { 
            infoMsg(IM::Moderate, "%s(%d)\n", 
              (*crrnt_node)->randomVariable->name().c_str(), 
              (*crrnt_node)->randomVariable->frame()); 
          }
        } 
        infoMsg(IM::Moderate, "--------------------------\n");

        best_list_cliques = cliques;
        best_weight = weight;
      } 
    } 

    //////////////////////////////////////////////////////////////////
    // Set up next edge configuration 
    //////////////////////////////////////////////////////////////////
    row = 0;    
    col = 1;

    //////////////////////////////////////////////////////////////
    // Find next empty edge 
    //////////////////////////////////////////////////////////////
    while ( ((adjacency[row][col] == EDGE) || 
             (adjacency[row][col] == FILL_IN)) &&
            (row < nmbr_nodes) ) {
 
      col++;
      if (col >= nmbr_nodes) {
        row++;
        col = row+1;
      }
    }

    //////////////////////////////////////////////////////////////
    // Set edge, erase prior edges 
    //////////////////////////////////////////////////////////////
    if (col >= nmbr_nodes) { 
      done = true;
    } 
    else {

      adjacency[row][col] = FILL_IN;
      triangulate_nodes[row].neighbors.push_back(&(triangulate_nodes[col]));
      triangulate_nodes[col].neighbors.push_back(&(triangulate_nodes[row]));

      i = 0;
      j = 1;

      while ((i != row) || (j != col)) {
        if (adjacency[i][j] == FILL_IN) {
          adjacency[i][j] = NONE;

          crrnt_node = find( triangulate_nodes[j].neighbors.begin(),
            triangulate_nodes[j].neighbors.end(), &triangulate_nodes[i] );
          triangulate_nodes[j].neighbors.erase(crrnt_node);

          crrnt_node = find( triangulate_nodes[i].neighbors.begin(),
            triangulate_nodes[i].neighbors.end(), &triangulate_nodes[j] );
          triangulate_nodes[i].neighbors.erase(crrnt_node);
        }

        j++;
        if (j >= nmbr_nodes) {
          i++;
          j = i+1;
        }
      }
    } 

    //////////////////////////////////////////////////////////////
    // Check if timer has expired 
    //////////////////////////////////////////////////////////////
    if (timer && timer->Expired()) { 
      infoMsg(IM::Tiny, 
        "Time expired before completion of exhaustive search\n");
      done = true;
    }

  }

  //////////////////////////////////////////////////////////////////////////
  // Convert to MaxClique and triangulate the RV structures
  //////////////////////////////////////////////////////////////////////////
  listVectorCliquetoVectorSetClique( best_list_cliques, best_cliques );
  fillAccordingToCliques( best_cliques );

  if (!triangulation_found) {
    vector<RV*> order;
    infoMsg(IM::Tiny, 
      "Exhaustive search exited before finding any triangulations\n");
    triangulateMaximumCardinalitySearch(nodes, best_cliques, order );
  }

  infoMsg(IM::Tiny, "---->Tested:  %d\n", crrnt_trial); 
}


/*-
 *-----------------------------------------------------------------------
 * isEliminationGraph 
 *   Determines if a triangulation is an elimination graph, this 
 *   algorithm is described in "Elimination is Not Enough: Non-Minimal 
 *   Triangulations for Graphical Models," by Chris Bartels and Jeff 
 *   Bilmes, UWEETR-2004-0010, June 2004 
 * 
 * Preconditions:
 *   none
 *
 * Postconditions:
 *   none
 *
 * Side Effects:
 *   none
 *
 * Results:
 *   Returns true if the triangulation in the current nodes can be created
 *   from the SavedGraph through elimination, false if it can not.
 *
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
isEliminationGraph
  (
  SavedGraph      orgnl_nghbrs,
  const set<RV*>& nodes
  )
{
  set<RV*> unprocessed_nodes;
  set<RV*> unprocessed_TG_nghbrs;
  set<RV*> unprocessed_G_nghbrs;
  
  set<RV*>::iterator crrnt_node;
  set<RV*>::iterator end_node;
  set<RV*>::iterator crrnt_unprcssd;
  set<RV*>::iterator end_unprcssd;
  SavedGraph::iterator crrnt_orgnl;
  SavedGraph::iterator end_orgnl;
  int      nmbr_fill_in; 
  bool     same_neighbors; 
  bool     elimination_graph; 

  for ( crrnt_node = nodes.begin(), 
        end_node = nodes.end();
        crrnt_node != end_node;
        crrnt_node++ ) {
    unprocessed_nodes.insert( *crrnt_node );
  }

  //////////////////////////////////////////////////////////////////////
  // Iterate until all nodes except one has been eliminated 
  //////////////////////////////////////////////////////////////////////
  elimination_graph = true;
  while ((unprocessed_nodes.size() > 1) &&
         (elimination_graph == true)) {

    //////////////////////////////////////////////////////////////////////
    // Check all uneliminated nodes to see if any are simplicial in T(G) 
    // and have the same neighbors in both G and T(G)
    //////////////////////////////////////////////////////////////////////
    for ( crrnt_unprcssd = unprocessed_nodes.begin(), 
          end_unprcssd = unprocessed_nodes.end();
          crrnt_unprcssd != end_unprcssd;
          crrnt_unprcssd++ ) {

      //////////////////////////////////////////////////////////////////////
      // Check if the current node is simplicial in T(G)
      //////////////////////////////////////////////////////////////////////
      unprocessed_TG_nghbrs.clear();
      set_intersection( (*crrnt_unprcssd)->neighbors.begin(),
        (*crrnt_unprcssd)->neighbors.end(), unprocessed_nodes.begin(), 
        unprocessed_nodes.end(), inserter(unprocessed_TG_nghbrs, 
        unprocessed_TG_nghbrs.end()) );

      nmbr_fill_in = computeFillIn( unprocessed_TG_nghbrs ); 

      //////////////////////////////////////////////////////////////////////
      // There will be zero fill in edges needed if node is simplicial, if 
      // so check if the node has same neighbors in G and T(G)
      //////////////////////////////////////////////////////////////////////
      if (nmbr_fill_in == 0) {

        //////////////////////////////////////////////////////////////////////
        // Find the simplicial node in the SavedGraph 
        //////////////////////////////////////////////////////////////////////
        for ( crrnt_orgnl = orgnl_nghbrs.begin(), 
              end_orgnl   = orgnl_nghbrs.end();
              crrnt_orgnl != end_orgnl;
              crrnt_orgnl++ ) {
          if ((*crrnt_orgnl).first == (*crrnt_unprcssd)) {
            break;
          }
        }
        assert(crrnt_orgnl != end_orgnl);

        //////////////////////////////////////////////////////////////////////
        // Find the uneliminated neighbors 
        //////////////////////////////////////////////////////////////////////
        unprocessed_G_nghbrs.clear();
        set_intersection( (*crrnt_orgnl).second.begin(),
          (*crrnt_orgnl).second.end(), unprocessed_nodes.begin(), 
          unprocessed_nodes.end(), inserter(unprocessed_G_nghbrs, 
          unprocessed_G_nghbrs.end()) );

        same_neighbors = equal( unprocessed_TG_nghbrs.begin(), 
          unprocessed_TG_nghbrs.end(), unprocessed_G_nghbrs.begin() );

        //////////////////////////////////////////////////////////////////////
        // If the neighbors in T(G) and G match, eliminate the node in both
        // graphs
        //////////////////////////////////////////////////////////////////////
        if (same_neighbors) {

          ////////////////////////////////////////////////////////////////////
          // Eliminate the node in T(G) by removing from unprocessed_nodes, 
          // the node is simplicial in T(G) so no additional processing is
          // needed
          ////////////////////////////////////////////////////////////////////
          unprocessed_nodes.erase( *crrnt_unprcssd ); 
           
          ////////////////////////////////////////////////////////////////////
          // Eliminate the node in G by connecting all of its neighbors 
          ////////////////////////////////////////////////////////////////////
          for ( crrnt_node = unprocessed_G_nghbrs.begin(), 
                end_node   = unprocessed_G_nghbrs.end(); 
                crrnt_node != end_node;
                crrnt_node++ ) {

            ////////////////////////////////////////////////////////////////
            // Find the current neighbor in the SavedGraph 
            ////////////////////////////////////////////////////////////////
            for ( crrnt_orgnl = orgnl_nghbrs.begin(), 
                  end_orgnl   = orgnl_nghbrs.end();
                  crrnt_orgnl != end_orgnl;
                  crrnt_orgnl++ ) {
              if ((*crrnt_orgnl).first == (*crrnt_node)) {
                break;
              }
            }
            assert(crrnt_orgnl != end_orgnl);

            ////////////////////////////////////////////////////////////////
            // Union the neighbors in the saved graph with the neighbors of 
            // the node being eliminated
            ////////////////////////////////////////////////////////////////
            set<RV*> new_neighbors;

            set_union( (*crrnt_orgnl).second.begin(), 
              (*crrnt_orgnl).second.end(), unprocessed_G_nghbrs.begin(), 
              unprocessed_G_nghbrs.end(), inserter(new_neighbors, 
              new_neighbors.end()) );

            new_neighbors.erase( *crrnt_node );
            (*crrnt_orgnl).second = new_neighbors;
          }

          ////////////////////////////////////////////////////////////////
          // Restart search of uneliminated nodes 
          ////////////////////////////////////////////////////////////////
          break;      
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////
    // If all nodes have been examined and none are both simplicial in T(G) 
    // and have the same neighbors in G and T(G), then this is not an 
    // elimination graph  
    ////////////////////////////////////////////////////////////////////////
    if (crrnt_unprcssd == end_unprcssd) {
      elimination_graph = false;
    }
  }

  return(elimination_graph);
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::triangulateElimination()
 *   Triangulate a set of nodes using an elimination order
 *    
 * Preconditions:
 *   Graph must be a valid undirected model. This means if the graph
 *   was originally a directed model, it must have been properly
 *   moralized and their 'neighbors' structure is valid. It is also
 *   assumed that the parents of each r.v. are valid but that they
 *   only point to variables within the set 'nodes' (i.e., parents
 *   must not point out of this set).
 *
 * Postconditions:
 *   Resulting graph is now triangulated.
 *
 * Side Effects:
 *   Changes rv's neighbors variables.
 *
 * Results:
 *     none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
triangulateElimination(// input: nodes to be triangulated
                       const set<RV*> nodes,
                       // elimination order 
                       vector<RV*> orderedNodes,  
                       // output: resulting max cliques
                       vector<MaxClique>& cliques
                       )
{
  // Keep ordered eliminated nodes as a set for easy
  // intersection, with other node sets.
  set<RV*> orderedNodesSet;

  // Triangulate and make cliques
  for (unsigned i=0;i<orderedNodes.size();i++) {
    RV* rv = orderedNodes[i];

    
    // connect all neighbors of r.v. excluding nodes in 'orderedNodesSet'.
    rv->connectNeighbors(orderedNodesSet); 
    

    // check here if this node + its neighbors is a subset
    // of previous maxcliques. If it is not a subset of any previous
    // maxclique, then this node and its neighbors is a 
    // new maxclique.
    set<RV*> candidateMaxClique;
    set_difference(rv->neighbors.begin(),rv->neighbors.end(),
                   orderedNodesSet.begin(),orderedNodesSet.end(),
                   inserter(candidateMaxClique,candidateMaxClique.end()));
    candidateMaxClique.insert(rv);
    bool is_max_clique = true;
    for (unsigned i=0;i < cliques.size(); i++) {
      if (includes(cliques[i].nodes.begin(),cliques[i].nodes.end(),
                   candidateMaxClique.begin(),candidateMaxClique.end())) {
        // then found a 'proven' maxclique that includes our current
        // candidate, so the candidate cannot be a maxclique
        is_max_clique = false;
        break;
      }
    }
    if (is_max_clique) {
      cliques.push_back(MaxClique(candidateMaxClique));
    }

    // insert node into ordered set
    orderedNodesSet.insert(rv);
  }
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::unrollAndTriangulate(th,int)
 *  Just unroll the graph a given number of times, triangulate it
 *  using the supplied heuristics, and report the results.
 *  This routine corresponds to unconstrained triangulation.
 *
 * Preconditions:
 *
 * Postconditions:
 *
 * Side Effects:
 *
 * Results:
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
unrollAndTriangulate(// triangulate heuristics
		     const string& tri_heur_str,  
		     // number of times it should be unrolled
		     const unsigned numTimes)
{
  TriangulateHeuristics tri_heur;
  parseTriHeuristicString(tri_heur_str,tri_heur);
  const set <RV*> emptySet;

  if (numTimes >= 0) {
    vector <RV*> rvs;
    map < RVInfo::rvParent, unsigned > pos;
    set <RV*> rvsSet;
    fp.unroll(numTimes,rvs,pos);
    if (message(Huge)) {
      // print out unrolled graph
      printRVSet(stdout,rvs);
    }
    for (unsigned i=0;i<rvs.size();i++) {
      rvs[i]->createNeighborsFromParentsChildren();
    }
    // add edges from any extra factors in .str file
    fp.addUndirectedFactorEdges(numTimes,rvs,pos);
    for (unsigned i=0;i<rvs.size();i++) {
      rvs[i]->moralize();
      rvsSet.insert(rvs[i]);
    }
    vector<MaxClique> cliques;
    SavedGraph orgnl_nghbrs;
    saveCurrentNeighbors(rvsSet,orgnl_nghbrs);
    string best_meth_str;
    double best_weight = DBL_MAX;
    triangulatePartition(rvsSet,false,emptySet,tri_heur,orgnl_nghbrs,cliques,best_meth_str,best_weight);
    unsigned maxSize = 0;
    float maxSizeCliqueWeight=0;
    float maxWeight = -1.0;
    float totalWeight = -1.0; // starting flag
    unsigned maxWeightCliqueSize=0;

    // TODO: just print out the resulting information for now. Ultimately
    // return the cliques and do inference with them.

    printf("Cliques from graph unrolled %d times\n",numTimes);
    for (unsigned i=0;i<cliques.size();i++) {

      float curWeight = MaxClique::computeWeight(cliques[i].nodes);
      if (curWeight > maxWeight) {
	maxWeight = curWeight;
	maxWeightCliqueSize = cliques[i].nodes.size();
      }
      if (totalWeight == -1.0)
	totalWeight = curWeight;
      else
	totalWeight = log10add(totalWeight,curWeight);
      if (cliques[i].nodes.size() > maxSize) {
	maxSize = cliques[i].nodes.size();
	maxSizeCliqueWeight = curWeight;
      }
      printf("%d : %ld  %f\n",i,
	     (unsigned long)cliques[i].nodes.size(),curWeight);
      for (set<RV*>::iterator j=cliques[i].nodes.begin();
	   j != cliques[i].nodes.end(); j++) {
	RV* rv = (*j);
	printf("   %s(%d)\n",rv->name().c_str(),rv->frame());
      }
    }
    printf("When unrolling %d times, max size clique = %d (with a weight of %f) and max weight of a clique = %f (with a size of %d). Total state space = %f\n",
	   numTimes,
	   maxSize,
	   maxSizeCliqueWeight,
	   maxWeight,
	   maxWeightCliqueSize,
	   totalWeight);
    printf("\n");
  }
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::ensurePartitionsAreChordal(gm_template,dieIfNot)
 *   ensure that the partitions in the given template are chordal, and die 
 *   with an error if not.
 *
 * Preconditions:
 *   Template should be fully instantiated
 *
 * Postconditions:
 *   Partitions in template are guaranteed to be chordal, if program has 
 *   not died.
 *
 * Side Effects:
 *   Re-writes the maxcliques in the current gm_template (i.e.,
 *   they'll still be maxcliques, but now they come from and are
 *   ordered by MCS).
 *
 * Results:
 *   none
 *-----------------------------------------------------------------------
 */
bool
BoundaryTriangulate::
ensurePartitionsAreChordal(GMTemplate& gm_template,
			   const bool dieIfNot)
{
  vector<triangulateNode>         triangulate_nodes; 
  list<vector<triangulateNode*> > list_cliques;
  vector<triangulateNode*>        order; 
  bool p_chordal;
  bool c_chordal;
  bool e_chordal;

  fillTriangulateNodeStructures( gm_template.P.nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  p_chordal = testZeroFillIn( order );

  triangulate_nodes.clear();
  fillTriangulateNodeStructures( gm_template.C.nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  c_chordal = testZeroFillIn( order );

  triangulate_nodes.clear();
  fillTriangulateNodeStructures( gm_template.E.nodes, triangulate_nodes );
  maximumCardinalitySearch( triangulate_nodes, list_cliques, order, false);
  e_chordal = testZeroFillIn( order );

  if (!p_chordal || !c_chordal || !e_chordal) {
    if (dieIfNot) {
      error("ERROR: Program exiting since the following partitions are not chordal:%s%s%s",
	    (p_chordal?"":" P"),
	    (c_chordal?"":" C"),
	    (e_chordal?"":" E"));
    } else {
      warning("ERROR: Problem with graph since the following partitions are not chordal:%s%s%s",
	    (p_chordal?"":" P"),
	    (c_chordal?"":" C"),
	    (e_chordal?"":" E"));
    }
    return false;
  }
  return true;
}


/*******************************************************************************
 *******************************************************************************
 *******************************************************************************
 **
 **        Support for Anytime Triangulation.
 **
 *******************************************************************************
 *******************************************************************************
 *******************************************************************************
 */


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::anyTimeTriangulate()
 *  Triangulate the P,C, and E partitions using an anytime procedure
 *
 * Preconditions:
 *   Each partition must corresond to a valid and separate GM. Each
 *   variable in each GM must have a valid parent and neighbor members
 *   and the parents/neighbors must only point to other members of a
 *   given partition.
 *   Current class timer must be a valid pointer to a timer.
 * 
 * Postconditions:
 *   Each of the partitions have been triangulated to the graph giving
 *   the best weight that was found. 
 *
 * Side Effects:
 *   Neighbor members of each random variable can be changed.
 *
 * Results:
 *   none
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate
::anyTimeTriangulate(GMTemplate& gm_template,
		     const bool jtWeight,
		     const bool doP,
		     const bool doC,
		     const bool doE)
{
  assert (timer != NULL);

  NghbrPairType nghbr_pair;
  SavedGraph orgnl_P_nghbrs;
  SavedGraph orgnl_C_nghbrs;
  SavedGraph orgnl_E_nghbrs;
  string best_P_method_str;
  string best_C_method_str;
  string best_E_method_str;
  double best_P_weight = DBL_MAX;
  double best_C_weight = DBL_MAX;
  double best_E_weight = DBL_MAX;
  const set <RV*> emptySet;

  ////////////////////////////////////////////////////////////////////////
  // Save the untriangulated graphs so that they can be quickly restored
  // for multiple iterations
  ////////////////////////////////////////////////////////////////////////
  if (doP) saveCurrentNeighbors(gm_template.P,orgnl_P_nghbrs);
  if (doC) saveCurrentNeighbors(gm_template.C,orgnl_C_nghbrs);
  if (doE) saveCurrentNeighbors(gm_template.E,orgnl_E_nghbrs);

  ////////////////////////////////////////////////////////////////////////
  // Triangulate using a basic heuristic so that something valid is in 
  // place just in case there is no time for anything else 
  ////////////////////////////////////////////////////////////////////////
  if (doP) {
    if (timer)
      timer->DisableTimer();
    infoMsg(IM::Tiny, "---\nPreliminary Triangulation of P:\n");
    setUpForP(gm_template);
    triangulatePartition( gm_template.P.nodes, jtWeight, 
			  gm_template.PCInterface_in_P,
			  "FWH", orgnl_P_nghbrs, gm_template.P.cliques,
			  gm_template.P.triMethod, best_P_weight );
    if (timer)
      timer->EnableTimer();
  }

  if (doC) {
    if (timer)
      timer->DisableTimer();
    infoMsg(IM::Tiny, "---\nPreliminary Triangulation of C:\n");
    setUpForC(gm_template);
    triangulatePartition( gm_template.C.nodes, jtWeight, 
			  gm_template.CEInterface_in_C,
			  "FWH", orgnl_C_nghbrs, gm_template.C.cliques,
			  gm_template.C.triMethod, best_C_weight );
    if (timer)
      timer->EnableTimer();
  }

  if (doE) {
    if (timer)
      timer->DisableTimer();
    infoMsg(IM::Tiny, "---\nPreliminary Triangulation of E:\n");
    setUpForE(gm_template);
    triangulatePartition( gm_template.E.nodes, jtWeight, 
			  emptySet,
			  "FWH", orgnl_E_nghbrs, gm_template.E.cliques, 
			  gm_template.E.triMethod, best_E_weight ); 
    if (timer)
      timer->EnableTimer();
  }

  ////////////////////////////////////////////////////////////////////////
  // Triangulate using a variety of heuristic searches 
  ////////////////////////////////////////////////////////////////////////

  // triangulate C first since that is the thing that gets unrolled.
  if (doC) {
    infoMsg(IM::Tiny, "---\nTriangulating C using Heuristics:\n");
    setUpForC(gm_template);
    triangulatePartition( gm_template.C.nodes, jtWeight, gm_template.CEInterface_in_C,
			  "heuristics", orgnl_C_nghbrs, gm_template.C.cliques, 
			  gm_template.C.triMethod, best_C_weight );
  }

  // we do E first since having a good E triangulation is often more
  // important than a good P. This is because we are doing
  // left-to-right based inference, starting at P, going through a
  // number of Cs, and ending at E. It is likely that the state space
  // of P will naturally be smaller since it is the start of
  // inference, but by the time we get to E, we are at full state
  // space. Therefore, we put a bit of priority on E over P.
  if (doE) { 
    infoMsg(IM::Tiny, "---\nTriangulating E using Heuristics:\n");
    setUpForE(gm_template);
    triangulatePartition( gm_template.E.nodes, jtWeight, emptySet, 
			  "heuristics", orgnl_E_nghbrs, gm_template.E.cliques, 
			  gm_template.E.triMethod, best_E_weight );
  }
 
  if (doP) { 
    infoMsg(IM::Tiny, "---\nTriangulating P using Heuristics:\n");
    setUpForP(gm_template);
    triangulatePartition( gm_template.P.nodes, jtWeight, gm_template.PCInterface_in_P, 
			  "heuristics", orgnl_P_nghbrs, gm_template.P.cliques, 
			  gm_template.P.triMethod, best_P_weight );
  }
  
  infoMsg(IM::Tiny, "Time Remaining: %d\n", (int)timer->SecondsLeft() ); 

  ////////////////////////////////////////////////////////////////////////
  // Triangulate using simulated annealing 
  ////////////////////////////////////////////////////////////////////////
  if (doC && timer->SecondsLeft() > 10) {
    // Do C first since it is more important.
    infoMsg(IM::Tiny, "---\nTriangulating C using Simulated Annealing:\n");
    setUpForC(gm_template);
    triangulatePartition( gm_template.C.nodes, jtWeight, gm_template.CEInterface_in_C,
			  "anneal", orgnl_C_nghbrs, gm_template.C.cliques, 
			  gm_template.C.triMethod, best_C_weight ); 

    infoMsg(IM::Tiny, "Time Remaining: %d\n", (int)timer->SecondsLeft() ); 
  }

  if (doE && timer->SecondsLeft() > 10) {
    infoMsg(IM::Tiny, "---\nTriangulating E using Simulated Annealing:\n");
    setUpForE(gm_template);
    triangulatePartition( gm_template.E.nodes, jtWeight, emptySet,
			  "anneal", orgnl_E_nghbrs, gm_template.E.cliques, 
			  gm_template.E.triMethod, best_E_weight ); 

    infoMsg(IM::Tiny, "Time Remaining: %d\n", (int)timer->SecondsLeft() ); 
  }

  if (doP && timer->SecondsLeft() > 10) {
    infoMsg(IM::Tiny, "---\nTriangulating P using Simulated Annealing:\n");
    setUpForP(gm_template);
    triangulatePartition( gm_template.P.nodes, jtWeight, gm_template.PCInterface_in_P,
			  "anneal", orgnl_P_nghbrs, gm_template.P.cliques, 
			  gm_template.P.triMethod, best_P_weight ); 

    infoMsg(IM::Tiny, "Time Remaining: %d\n", 
      (int)timer->SecondsLeft()); 
  }


  ////////////////////////////////////////////////////////////////////////
  // Triangulate using exhaustive search
  ////////////////////////////////////////////////////////////////////////

  if (doC && timer->SecondsLeft() > 10) {
    infoMsg(IM::Tiny, "Triangulating C using Exhaustive Search:\n");
    setUpForC(gm_template);    
    triangulatePartition( gm_template.C.nodes, jtWeight, gm_template.CEInterface_in_C,
			  "exhaustive", orgnl_C_nghbrs, gm_template.C.cliques, 
			  gm_template.C.triMethod, best_C_weight ); 

    infoMsg(IM::Tiny, "Time Remaining: %d\n", (int)timer->SecondsLeft() ); 
  }

  if (doE && timer->SecondsLeft() > 10) {
    infoMsg(IM::Tiny, "Triangulating E using Exhaustive Search:\n");
    setUpForE(gm_template);
    triangulatePartition( gm_template.E.nodes, jtWeight, emptySet,
			  "exhaustive", orgnl_E_nghbrs, gm_template.E.cliques, 
			  gm_template.E.triMethod, best_E_weight ); 

    infoMsg(IM::Tiny, "Time Remaining: %d\n", (int)timer->SecondsLeft() ); 
  }


  if (doP && timer->SecondsLeft() > 10) {
    infoMsg(IM::Tiny, "Triangulating P using Exhaustive Search:\n");
    setUpForP(gm_template);
    triangulatePartition( gm_template.P.nodes, jtWeight, gm_template.PCInterface_in_P,
			  "exhaustive", orgnl_P_nghbrs, gm_template.P.cliques, 
			  gm_template.P.triMethod, best_P_weight ); 

    infoMsg(IM::Tiny, "Time Remaining: %d\n", (int)timer->SecondsLeft() ); 
  }


  ////////////////////////////////////////////////////////////////////////
  // Return with the best triangulations found, which is
  // be stored within the template at this point.
  ////////////////////////////////////////////////////////////////////////
  if (doP) restoreNeighbors(orgnl_P_nghbrs);
  if (doC) restoreNeighbors(orgnl_C_nghbrs);
  if (doE) restoreNeighbors(orgnl_E_nghbrs);
  gm_template.triangulatePartitionsByCliqueCompletion();

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::tryEliminationHeuristics
 *  Triangulate a partition using elimination with multiple iterations of 
 *  a variety of heuristics.  Use overal graph weight to judge ultimate 
 *  triangulation quality.
 *
 *  There are two ways to judget the quality of the triangulation. 
 *  If jtWeight = false, use just sum of weights in each resulting clique.
 *  If jtWeight = true, approximate JT quality by forming JT and computing 
 *  JT weight.
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   members in the set. 
 * 
 * Postconditions:
 *   The triangulation with the best weight is stored in 'cliques' 
 *
 * Side Effects:
 *   The partition is triangulated to some triangulation, not 
 *   necessarily corresponding to the best order.  Neighbor members of 
 *   each random variable can be changed.
 *
 * Results:
 *   The best weight found 
 *-----------------------------------------------------------------------
 */
double 
BoundaryTriangulate::
tryEliminationHeuristics(
  const set<RV*>&  nodes,
  const bool                   jtWeight,
  const set<RV*>&  nrmc,         // nrmc = nodes root must contain
  SavedGraph&                  orgnl_nghbrs,
  vector<MaxClique>&           best_cliques, // output: resulting max cliques
  string&                      best_method,  // output: best method name 
  double&                      best_weight,  // weight of best
  string                       best_method_prefix 
  )
{
  ///////////////////////////////////////////////////////////////////////////// 
  // Try Weight, Fill, Size
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-WFS-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-HWFS-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Fill, Weight, Size 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-FWS-3",  
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-HFWS-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Weight
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-W-3",  
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-HW-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-TW-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Fill 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-F-3",  
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-HF-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-TF-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Size 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-S-3",  
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-HS-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"20-ST-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Time
  triangulatePartition( nodes, jtWeight, nrmc, 
			"100-T-3",  
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-TS-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-TW-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-TF-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Position
  triangulatePartition( nodes, jtWeight, nrmc, 
			"P",  
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-WP-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-FP-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-SP-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try Maximum Cardinality Search
  triangulatePartition( nodes, jtWeight, nrmc, 
			"50-MCS", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try purely random
  triangulatePartition( nodes, jtWeight, nrmc, 
			"500-R-1", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try hints only 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"30-H-3", 
			orgnl_nghbrs, best_cliques, best_method, best_weight, best_method_prefix );

  return(best_weight);
}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::tryNonEliminationHeuristics
 *  Triangulate a partition using elimination with multiple iterations of 
 *  a variety of heuristics.  All of the heuristics in this proceedure 
 *  have the potential to give triangulations not obtainable through any
 *  elimination order.  Use overal graph weight to judge ultimate 
 *  triangulation quality.
 *
 *  There are two ways to judget the quality of the triangulation. 
 *  If jtWeight = false, use just sum of weights in each resulting clique.
 *  If jtWeight = true, approximate JT quality by forming JT and computing 
 *    JT weight.
 *
 * Preconditions:
 *   Each variable in the set of nodes must have valid parent and 
 *   neighbor members and the parents/neighbors must only point to other 
 *   members in the set. 
 * 
 * Postconditions:
 *   The triangulation with the best weight is stored in 'cliques' 
 *
 * Side Effects:
 *   The partition is triangulated to some triangulation, not 
 *   necessarily corresponding to the best order.  Neighbor members of 
 *   each random variable can be changed.
 *
 * Results:
 *   The best weight found 
 *-----------------------------------------------------------------------
 */
double 
BoundaryTriangulate::
tryNonEliminationHeuristics(
  const set<RV*>& nodes,
  const bool                  jtWeight,
  const set<RV*>& nrmc,         // nrmc = nodes root must contain
  SavedGraph&                 orgnl_nghbrs,
  vector<MaxClique>&          cliques,
  string&                     best_method,
  double&                     best_weight   
  )
{
  ///////////////////////////////////////////////////////////////////////////// 
  // Try simply completing the partition (this can work well if many 
  // deterministic nodes exist in the partition)
  triangulatePartition( nodes, jtWeight, nrmc, 
			"completed", orgnl_nghbrs, cliques, 
			best_method, best_weight );

  ///////////////////////////////////////////////////////////////////////////// 
  // Completing the nodes within each frame, plus the left interface 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"completeframeleft", orgnl_nghbrs, cliques, 
			best_method, best_weight );

  ///////////////////////////////////////////////////////////////////////////// 
  // Completing the nodes within each frame, plus the right interface 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"completeframeright", orgnl_nghbrs, cliques, 
			best_method, best_weight );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try adding all ancestral edges, followed by elimination heuristics 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"pre-edge-all-elimination-heuristics", 
			orgnl_nghbrs, cliques, best_method, best_weight );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try adding locally optimal ancestral edges, followed by elimination 
  // heuristics 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"pre-edge-lo-elimination-heuristics", 
			orgnl_nghbrs, cliques, best_method, best_weight );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try adding random optimal ancestral edges, followed by elimination 
  // heuristics 
  triangulatePartition( nodes, jtWeight, nrmc, 
			"10-pre-edge-random-elimination-heuristics", 
			orgnl_nghbrs, cliques, best_method, best_weight );

  ///////////////////////////////////////////////////////////////////////////// 
  // Try frontier algorithm
  triangulatePartition( nodes, jtWeight, nrmc, 
			"500-frontier", 
			orgnl_nghbrs, cliques, best_method, best_weight );

  return(best_weight);
}


/*******************************************************************************
 *******************************************************************************
 *******************************************************************************
 **
 **         Boundary Algorithm Routines
 **
 *******************************************************************************
 *******************************************************************************
 *******************************************************************************
 */



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::interfaceScore()
 *      Compute the 'score' of a set of variables that are to be a
 *      candidate interface. In the best of cases, it is a simple easy
 *      score based on the vars size, fill-in, or weight.  In the
 *      "worst" of cases, the score is based on an entire
 *      triangulation of the variables that would result from this
 *      interface.
 *      
 *
 * Preconditions:
 *      - set of variables must be instantiated and their
 *        neighbors member been filled in.
 *      - score heuristic vector instantiated.
 *
 * Postconditions:
 *      - score in vector is returned.
 *
 * Side Effects:
 *      none
 *
 * Results:
 *     - score in vector returned.
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::interfaceScore(
 // the interface heuristic used to score the interface
 const vector<BoundaryHeuristic>& bnd_heur_v,
 // the interface itself
 const set<RV*>& C_l,
 // --------------------------------------------------------------
 // The next 8 input arguments are used only with the optimal
 // interface algorithm when the IH_MIN_MAX_C_CLIQUE or
 // IH_MIN_MAX_CLIQUE heuristics are used:
 // triangulation heuristic
 // Variables to the left (or right) of the interface
 const set<RV*>& left_C_l,
 // the triangulation heuristic 
 const TriangulateHeuristics& tri_heur,
 // The network unrolled 1 time
 const set<RV*>& P_u1,
 const set<RV*>& C1_u1,
 const set<RV*>& Cextra_u1,
 const set<RV*>& C2_u1,
 const set<RV*>& E_u1,
 // Mappings from C2 in the twice unrolled network to C1 and C2
 // in the once unrolled network.
 // (these next 2 should be const, but there is no "op[] const")
 map < RV*, RV* >& C2_u2_to_C1_u1,
 map < RV*, RV* >& C2_u2_to_C2_u1,
 // --------------------------------------------------------------
 // output score
 vector<float>& score)
{

  if (message(Moderate)) {
    set<RV*>::iterator i;    
    printf("  --- Cur Interface:");
    printRVSet(stdout,C_l);
    if (message(VerbosityLevels(Moderate + (Increment>>1)))) {
      // also print out left_C_l
      printf("  Left of C_l:");
      printRVSet(stdout,left_C_l);
    }
    printf("\n");
  }
  score.clear();
  for (unsigned fhi=0;fhi<bnd_heur_v.size();fhi++) {
    const BoundaryHeuristic fh = bnd_heur_v[fhi];
    if (fh == IH_MIN_WEIGHT || fh == IH_MIN_WEIGHT_NO_D) {
      float tmp_weight = MaxClique::computeWeight(C_l,NULL,
				       (fh == IH_MIN_WEIGHT));
      score.push_back(tmp_weight);
      infoMsg(Low,"  Interface Score: set has weight = %f\n",tmp_weight);
    } else if (fh == IH_MIN_FILLIN) {
      int fill_in = computeFillIn(C_l);
      score.push_back((float)fill_in);
      infoMsg(Low,"  Interface Score: set has fill_in = %d\n",fill_in);
    } else if (fh == IH_MIN_SIZE) {
      score.push_back((float)C_l.size());
      infoMsg(Low,"  Interface Score: set has size = %d\n",
	      C_l.size());
    } else if (fh == IH_MIN_MIN_POSITION_IN_FILE) {
      set<RV*>::iterator i;    
      unsigned val = ~0x0;
      for (i=C_l.begin();i!=C_l.end();i++) {
	if (unsigned((*i)->rv_info.variablePositionInStrFile) < val)
	  val = (*i)->rv_info.variablePositionInStrFile;
      }
      score.push_back((float)val);
      infoMsg(Low,"  Interface Score: set has min pos = %d\n",val);
    } else if (fh == IH_MIN_MAX_POSITION_IN_FILE) {
      set<RV*>::iterator i;    
      unsigned val = 0;
      for (i=C_l.begin();i!=C_l.end();i++) {
	if (unsigned((*i)->rv_info.variablePositionInStrFile) > val)
	  val = (*i)->rv_info.variablePositionInStrFile;
      }
      score.push_back((float)val);
      infoMsg(Low,"  Interface Score: set has max pos = %d\n",val);
    } else if (fh == IH_MIN_MIN_TIMEFRAME) {
      set<RV*>::iterator i;    
      unsigned val = ~0x0;
      for (i=C_l.begin();i!=C_l.end();i++) {
	if ((*i)->frame() < val)
	  val = (*i)->frame();
      }
      score.push_back((float)val);
      infoMsg(Low,"  Interface Score: set has min timeframe = %d\n",val);
    } else if (fh == IH_MIN_MAX_TIMEFRAME) {
      set<RV*>::iterator i;    
      unsigned val = 0;
      for (i=C_l.begin();i!=C_l.end();i++) {
	if ((*i)->frame() > val)
	  val = (*i)->frame();
      }
      score.push_back((float)val);
      infoMsg(Low,"  Interface Score: set has max timeframe = %d\n",val);
    } else if (fh == IH_MIN_MIN_HINT) {
      set<RV*>::iterator i;    
      float val = MAXFLOAT;
      for (i=C_l.begin();i!=C_l.end();i++) {
	if ((*i)->rv_info.eliminationOrderHint < val)
	  val = (*i)->rv_info.eliminationOrderHint;
      }
      score.push_back(val);
      infoMsg(Low,"  Interface Score: set has min hint = %f\n",val);
    } else if (fh == IH_MIN_MAX_HINT) {
      set<RV*>::iterator i;    
      float val = -MAXFLOAT;
      for (i=C_l.begin();i!=C_l.end();i++) {
	if ((*i)->rv_info.eliminationOrderHint > val)
	  val = (*i)->rv_info.eliminationOrderHint;
      }
      score.push_back(val);
      infoMsg(Low,"  Interface Score: set has max hint = %f\n",val);
    } else if (fh == IH_MIN_MAX_CLIQUE || fh == IH_MIN_MAX_C_CLIQUE ||
	       fh == IH_MIN_STATE_SPACE || fh == IH_MIN_C_STATE_SPACE) {
      // This is the expensive one, need to form a set of partitions,
      // given the current interface, triangulate that partition set,
      // and then compute the score of the worst clique, and fill the
      // score variable above with this worst scoring clique (i.e., we
      // find the interface that has the best worst-case performance.

      // TODO: some of these values could be cached to speed this
      // up a bit.

      GMTemplate gm_template(fp,M,S);

      constructInterfacePartitions(P_u1,
				   C1_u1,
				   Cextra_u1,
				   C2_u1,
				   E_u1,
				   C2_u2_to_C1_u1,
				   C2_u2_to_C2_u1,
				   left_C_l,
				   C_l,
				   gm_template);


      if (fh == IH_MIN_MAX_CLIQUE || fh == IH_MIN_STATE_SPACE) {
	// then we do the entire graph, all three partitions
	SavedGraph orgnl_P_nghbrs;
	SavedGraph orgnl_C_nghbrs;
	SavedGraph orgnl_E_nghbrs;
	string best_P_method_str;
	string best_C_method_str;
	string best_E_method_str;
	double best_P_weight = DBL_MAX;
	double best_C_weight = DBL_MAX;
	double best_E_weight = DBL_MAX;

	saveCurrentNeighbors(gm_template.P,orgnl_P_nghbrs);
	saveCurrentNeighbors(gm_template.C,orgnl_C_nghbrs);
	saveCurrentNeighbors(gm_template.E,orgnl_E_nghbrs);

	// TODO: use version that includes jtWeight (this means more than setting arg to true)
	triangulatePartitionWeight(gm_template.P.nodes,tri_heur,orgnl_P_nghbrs,gm_template.P.cliques,
				   gm_template.P.triMethod,best_P_weight);
	triangulatePartitionWeight(gm_template.C.nodes,tri_heur,orgnl_C_nghbrs,gm_template.C.cliques,
				   gm_template.C.triMethod,best_C_weight);
	triangulatePartitionWeight(gm_template.E.nodes,tri_heur,orgnl_E_nghbrs,gm_template.E.cliques,
				   gm_template.E.triMethod,best_E_weight);

	if (fh == IH_MIN_STATE_SPACE) {
	  // just sum up the weights
	  double weight = best_P_weight;
	  weight = log10add(weight,best_C_weight);
	  weight = log10add(weight,best_E_weight);
	  score.push_back(weight);
	  infoMsg(Low,"  Interface Score: resulting graph with this interface has total weight = %f\n",
		  weight);
	} else {
	  // need to find the clique with max weight.
	  float maxWeight = -1.0;
	  for (unsigned i=0;i<gm_template.P.cliques.size();i++) {
	    float curWeight = MaxClique::computeWeight(gm_template.P.cliques[i].nodes);
	    if (curWeight > maxWeight) maxWeight = curWeight;
	  }
	  for (unsigned i=0;i<gm_template.C.cliques.size();i++) {
	    float curWeight = MaxClique::computeWeight(gm_template.C.cliques[i].nodes);
	    if (curWeight > maxWeight) maxWeight = curWeight;
	  }
	  for (unsigned i=0;i<gm_template.E.cliques.size();i++) {
	    float curWeight = MaxClique::computeWeight(gm_template.E.cliques[i].nodes);
	    if (curWeight > maxWeight) maxWeight = curWeight;
	  }
	  score.push_back(maxWeight);
	  infoMsg(Low,"  Interface Score: with this interface, resulting graph has largest clique weight = %f\n",
		  maxWeight);
	}
      } else {
	// then we do almost same, but just on C
	SavedGraph orgnl_C_nghbrs;
	string best_C_method_str;
	double best_C_weight = DBL_MAX;
	saveCurrentNeighbors(gm_template.C,orgnl_C_nghbrs);
	// TODO: use version that includes jtWeight (this means more than setting arg to true)
	setUpForC(gm_template);
	triangulatePartitionWeight(gm_template.C.nodes,tri_heur,orgnl_C_nghbrs,gm_template.C.cliques,best_C_method_str,best_C_weight);

	if (fh == IH_MIN_C_STATE_SPACE) {
	  // just sum up the weights
	  score.push_back(best_C_weight);
	  infoMsg(Low,"  Interface Score: resulting graph with this interface has total C-weight = %f\n",
		  best_C_weight);
	} else {
	  // need to find the clique with max weight.
	  float maxWeight = -1.0;
	  for (unsigned i=0;i<gm_template.C.cliques.size();i++) {
	    float curWeight = MaxClique::computeWeight(gm_template.C.cliques[i].nodes);
	    if (curWeight > maxWeight) maxWeight = curWeight;
	  }
	  score.push_back(maxWeight);
	  infoMsg(Low,"  Interface Score: with this interface, resulting C partition has largest clique weight = %f\n",
		  maxWeight);
	}
      }

    } else
      warning("Warning: invalid variable set score given. Ignored\n");
  }

  return;
}



/*-
 *-----------------------------------------------------------------------
 * bool BoundaryTriangulate::validInterfaceDefinition()
 *
 * check that the basic definitions (as given by the template) will be
 * valid for the boundary that will be computed for this particular
 * partitioning set.  Specifically, we make sure that the following
 * will have the same interfaces (in the left interface case):
 *   1) between P' and C'E'
 *   2) between P'C' and E'
 *   3) between P' and E'
 *
 *  A simple way to write this (using the user-specified non-primed
 *  partitions, P, C and E) is to say that: 
 *  In the left interface case (with M=S=1), we must have:
 *
 *    [P | C E] == [P | C C E] == [P C | C E]
 *
 *  What this means is this. The vertical bar '|' cuts the graph (via
 *  edges) into a left and a right portion.  In the left interface
 *  case, the nodes on the right of the edge cut must be the same
 *  (relatively) with respect to each other (i.e., they are the same
 *  nodes, perhaps all related by a shift by S*numframes(C)).  Note
 *  that when P is empty, we don't need to check the interface at all
 *  since we are guaranteed E does not reach to the left beyond one C,
 *  so there is only one total interface, The C C interface, in C | C
 *  E.
 *
 *  In the right interface case (with M=S=1), checked by reversing this 
 *  routines arguments, we must have:
 *
 *    [P C | E] == [P C C | E] == [P C | C E]
 *
 *  Here, we check that the nodes on the left of the cut are relatively the same.
 *
 * The above is the only graphical restriction paced on the template
 * (other than the obvious no directed cycles), so that we can use the
 * case of unrolling by zero (i.e., by not using C').  This is done so
 * that the boundary that the boundary algorithm returns,used to
 * compute the interface for the P-C boundary, the C-C boundary, and
 * the C-E boundary, will be valid.
 *
 *  As an example, we can now have a variable in E ask for a parent in,
 *  say, P, if the above restrictions are followed (which might means
 *  that C has to ask for the same variable in P, and that the variable
 *  in P being asked for also exists in C).
 *
 *  Here is a complete picture of the above, for arbitrary M>=1 and S>=1.
 *
 *  Left interface:
 *	    [ P | C(1) C(2) ... C(M) E ] 
 *	    [ P | C(1)  C(2)   ...     C(M+S) E ]
 *	    [ P  C(1) ...  C(S) | C(S+1) ... C(M+S) E ]
 *
 *  Right interface:
 *	    [ P C(1) C(2) ... C(M) | E ]
 *	    [ P C(1)  C(2)   ...     C(M+S) | E ]
 *	    [ P C(1) ...  C(S) | C(S+1) ... C(M+S) E ]
 *
 *  Therefore, you can increase M and/or S to relax further the restriction above
 *  (but at the cost of more restricted length T).
 * 
 * NOTE: This routine checks that this will be valid template defined
 * for the LEFT INTERFACE CASE ONLY. Call with mirrored arguments for the
 * right interface case.
 *
 *
 * Preconditions:
 *     - P, C1, Cextra,C2, and E must be validlly defined for a
 *     template unrolled (M+S-1) times. These correspond to the
 *     partitions before any new boundary information has been
 *     computed. See documentation in
 *     BoundaryTriangulate::findPartitions().
 *    - all of P,C1,Cextra,C2, and 2 set arguments *must* contain RVs that come from the
 *      same basic unrolling, and that unrolling must correspond to
 *      what unroll returns in args 'rvs' and 'pos'.
 *    - all of P0, C0, and E0 are from the same unrolling (0 unrolling of the partitions)
 *      with RVs corresponding to rvs0,pos0 returned by unroll().
 *
 * Postconditions:
 *      - none
 *
 * Side Effects:
 *      none
 *
 * Results:
 *     - condition of valid template.
 *
 */
bool BoundaryTriangulate::validInterfaceDefinition(const set<RV*> &P,
						   const set<RV*> &C1,
						   const set<RV*> &Cextra,
						   const set<RV*> &C2,
						   const set<RV*> &E,
						   const int S,
						   const vector <RV*>& rvs,
						   // should be const, but need member pos.find().
						   map < RVInfo::rvParent, unsigned >& pos,
						   const set<RV*> &P0,
						   const set<RV*> &C0,
						   const set<RV*> &E0,
						   const vector <RV*>& rvs0,
						   map < RVInfo::rvParent, unsigned >& pos0,
						   const bool leftInterface)
{
  
  const char* interfaceStr = (leftInterface? "LEFT" : "RIGHT");

  if (message(Max)) {
    printf("==== %s Interface case: Checking template for valid interface\nP:",interfaceStr);
    printRVSet(stdout,P);
    // printf("P:");printRVSet(stdout,P);
    printf("C1:");printRVSet(stdout,C1);
    printf("Cextra:");printRVSet(stdout,Cextra);
    printf("C2:");printRVSet(stdout,C2);
    printf("E:");printRVSet(stdout,E);
  }

  // If P is empty, then there is only one interface, and it will
  // always be valid as long as the network is valid.
  if (P.size() == 0)
    return true;


  // First compute P', C', and E', the basic non-boundary partitions.
  // Note: 
  //   P' = P
  //   C' = (C1 \ C2) U Cxtra
  //   E' = C2 U E
  const set<RV*>& Pp = P;
  set<RV*> Cp;
  if (Cextra.size() > 0) {
    // if Cextra has anything in it,
    // we are guaranteed that C1 has no intersection with C2
    set_union(C1.begin(),C1.end(),
	      Cextra.begin(),Cextra.end(),
	      inserter(Cp,Cp.end()));
  } else {
    set_difference(C1.begin(),C1.end(),
		   C2.begin(),C2.end(),
		   inserter(Cp,Cp.end()));
  }
  set<RV*> Ep;
  set_union(C2.begin(),C2.end(),
	    E.begin(),E.end(),
	    inserter(Ep,Ep.end()));

  if (message(Max)) {
    printf("P':");printRVSet(stdout,Pp);
    printf("C':");printRVSet(stdout,Cp);
    printf("E':");printRVSet(stdout,Ep);
  }

  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // Compute union(Cp,Ep)'s left interface to Pp
  set<RV*> UR_pli;
  // first compute union
  set<RV*> union_right;
  set_union(Cp.begin(),Cp.end(),
	    Ep.begin(),Ep.end(),
	    inserter(union_right,union_right.end()));
  // go through through set Pp, and pick out all neighbors of variables
  // in set Pp that live in union_right, and these neighbors are UR_pli.
  set<RV*>::iterator p_iter;
  for (p_iter = Pp.begin(); p_iter != Pp.end(); p_iter ++) {
    RV *cur_rv = (*p_iter);
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     union_right.begin(),union_right.end(),
		     inserter(UR_pli, UR_pli.end()));
  }

  if (message(Max)) {
    printf("Union(C',E')'s li to P': ");
    printRVSet(stdout,UR_pli);
  }

  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // Compute Ep's left interface to union(Pp,Cp) (both Pp and Cp)
  set<RV*> Ep_uli;
  // first compute union
  set<RV*> union_left;
  set_union(Cp.begin(),Cp.end(),
	    Pp.begin(),Pp.end(),
	    inserter(union_left,union_left.end()));
  // go through through set union_left, and pick out all neighbors of variables
  // in set union_left that live in Ep, and these neighbors are Ep_uli.
  set<RV*>::iterator u_iter;
  for (u_iter = union_left.begin(); u_iter != union_left.end(); u_iter ++) {
    RV *cur_rv = (*u_iter);
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     Ep.begin(),Ep.end(),
		     inserter(Ep_uli,Ep_uli.end()));
  }

  if (message(Max)) {
    printf("E's li to Union(P',C'): ");
    printRVSet(stdout,Ep_uli);
  }

  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // Compute Ep's interface to P' directly. We do this by
  // using the 0-unrolled RVs that have been passed in.
  set<RV*> Ep0;
  set_union(C0.begin(),C0.end(),
	    E0.begin(),E0.end(),
	    inserter(Ep0,Ep0.end()));
  set<RV*> Ep0_pli;
  // go through through set P0, and pick out all neighbors of variables
  // in set P0 that live in Ep0, and these neighbors are Ep0_pli.
  for (p_iter = P0.begin(); p_iter != P0.end(); p_iter ++) {
    RV *cur_rv = (*p_iter);
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     Ep0.begin(),Ep0.end(),
		     inserter(Ep0_pli, Ep0_pli.end()));
  }
  if (message(Max)) {
    printf("E'0's li to P'0: ");
    printRVSet(stdout,Ep0_pli);
  }

  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////

  if (UR_pli.size() != Ep_uli.size() || 
      Ep_uli.size() != Ep0_pli.size()) {
    if (message(Max)) {
      printf("INTERFACE FAILED: size(LI of union(C',E') to P')=%ld, size(LI of E' to union(P',C'))=%ld, size(LI of E' to P')=%ld\n",
	     (unsigned long)UR_pli.size(),
	     (unsigned long)Ep_uli.size(),
	     (unsigned long)Ep0_pli.size());
      printf("LI of union(C',E') to P': ");printRVSet(stdout,UR_pli);
      printf("LI of E' to union(P',C'): ");printRVSet(stdout,Ep_uli);
      printf("LI of E' to P': ");printRVSet(stdout,Ep0_pli);
    }
    return false;
  } else if (message(Max+50)) {
    printf("INTERFACE SIZE SUCCEEDED: size(LI of union(C',E') to P')=%ld, size(LI of E' to union(P',C'))=%ld, size(LI of E' to P')=%ld\n",
	   (unsigned long)UR_pli.size(),
	   (unsigned long)Ep_uli.size(),
	   (unsigned long)Ep0_pli.size());
    printf("LI of union(C',E') to P': ");printRVSet(stdout,UR_pli);
    printf("LI of E' to union(P',C'): ");printRVSet(stdout,Ep_uli);
    printf("LI of E' to P': ");printRVSet(stdout,Ep0_pli);    
  }

  // they have the same size, so now need to make sure they
  // are the same variables.

  // Checking that the union_right <-> Pp interface is the
  // same as the Ep <--> union_left interface is easy, since
  // we can just shift the first one to the right and check
  // for set equality.
  set<RV*> UR_pli_r_shifted = getRVSet(rvs,pos,UR_pli,(leftInterface?(1):(-1))*S*fp.numFramesInC(),false);
  if (UR_pli_r_shifted != Ep_uli) {
    if (message(Max)) {
      printf("%s INTERFACE FAILED (UR_pli_r_shifted(by %d) != Ep_uli)\n",
	     interfaceStr,
	     (leftInterface?(1):(-1))*S*fp.numFramesInC());
      printf("UR_pli_r_shifted:");
      printRVSet(stdout,UR_pli_r_shifted);
    }
    return false;
  }

  // Next, we must check that Ep's direct LI to Pp is the same as Ep's
  // left interface to union left. We do this by creating a version of
  // Ep0_pli but shifted right and in the right space, and then
  // compare.
  set<RV*> Ep0_pli_shifted = getRVSet(rvs,pos,Ep0_pli,(leftInterface?1:0)*S*fp.numFramesInC(),false);
  // next check that they are the same.
  if (Ep0_pli_shifted != Ep_uli) {
    if (message(Max)) {
      printf("%s INTERFACE FAILED (Ep0_pli_shifted(by %d) != Ep_uli)\n",
	     interfaceStr,
	     (leftInterface?(1):(0))*S*fp.numFramesInC());
      printf("Ep0_pli_shifted:");
      printRVSet(stdout,Ep0_pli_shifted);
    }
    return false;
  }


  return true;
}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::computeSMarkovAugmentation()
 *      Given the partitions Ps, Cs1, Cs2, and Es, where Cs1 and Cs2 are 
 *      both S chunks long, and where these partitions come from a graph
 *      unrolled by 2*S-1 frames, compute the augmentation to Cs so
 *      that the S-Markov property holds, namely that we are guaranteed
 *      that:
 *              Cs1 \indep Es | Cs2 
 *              Cs2 \indep Ps | Cs1
 *
 *      once the augmentation is computed and applied to each of Cs1 and Cs2.
 * 
 * Preconditions:
 *      Graph must be from unrolled 2*S-1 times, and each partition
 *      must come from the same underlying unrolling. Partitions
 *      must be valid (i.e., pass the validInterfaceDefinition() check). 
 *
 * Postconditions:
 *      Adjustment is placed in argument.
 *
 * Side Effects:
 *      Could modify Cs1 and Cs2.
 *
 * Results:
 *      returns augmentation.
 *
 *-----------------------------------------------------------------------
 */
void BoundaryTriangulate::computeSMarkovAugmentation(const vector <RV*>& rvs,
						     map < RVInfo::rvParent, unsigned >& pos,
						     const set<RV*>& Ps,
						     set<RV*>& Cs1,
						     const unsigned firstFrameCs1,
						     set<RV*>& Cextra,
						     set<RV*>& Cs2,
						     const unsigned firstFrameCs2,
						     const set<RV*>& Es,
						     set < RVInfo::rvParent >& augmentation)
{

  // first, go through Cs1 and any neighbors that are in Es that are
  // not in Cs2, we add them to Cs2 and to augmentation (relative to the
  // beginning of Cs2).
  if (Es.size() > 0) {
    set<RV*> Cunion;
    set_union(Cs1.begin(),Cs1.end(),
	      Cextra.begin(),Cextra.end(),
	      inserter(Cunion,Cunion.end()));
    set<RV*>::iterator cs1_it;
    for (cs1_it = Cunion.begin();cs1_it != Cunion.end(); cs1_it++) {
      RV* rv = (*cs1_it);
      set<RV*> tmp;
      set_intersection(rv->neighbors.begin(),rv->neighbors.end(),
		       Es.begin(),Es.end(),
		       inserter(tmp,tmp.end()));
      // printf("tmp.size() = %d\n",tmp.size());

      if (tmp.size() == 0)
	continue;
      set<RV*> rvEsinter;    
      set_difference(tmp.begin(),tmp.end(),
		     Cs2.begin(),Cs2.end(),
		     inserter(rvEsinter,rvEsinter.end()));
      // printf("rvEsinter.size()=%d\n",rvEsinter.size());

      if (rvEsinter.size() > 0) {
	// ok, then there is stuff that needs to be augmented.
	set<RV*>::iterator intr_it;
	for (intr_it = rvEsinter.begin(); intr_it != rvEsinter.end(); intr_it ++) {
	  RV* intr_rv = (*intr_it);
	  // compute the position relative to the beginning of Cs2 and added it
	  // to the augmentation.
	  RVInfo::rvParent pp(intr_rv->name(),intr_rv->frame()-firstFrameCs2);
	  infoMsg(Max,"Cu1<->Es: Adding %s(%d) to augmentation\n",pp.first.c_str(),pp.second);
	  augmentation.insert(pp);
	  // now we need to add it to Cs2 so that we don't add the same augmentation
	  // (but a different STL object) to 'augmentation' again. Add the absolute
	  // position.
	  Cs2.insert(intr_rv);
	}
      }
    }
  
    // add augmentation to Cs1
    augmentToAbideBySMarkov(rvs,pos,augmentation,Cs1,firstFrameCs1);
  }

  // TODO: below is symmetric. Could just call this routine twice with
  // reversed arguments.

  // Next, go through Cs2 and any neighbors that are in Ps that are not
  // in Cs1, we add them to cs1 and to augmentation (relative to the beginning
  // of Cs1)
  if (Ps.size() > 0) {
    set<RV*> Cunion;
    set_union(Cs2.begin(),Cs2.end(),
	      Cextra.begin(),Cextra.end(),
	      inserter(Cunion,Cunion.end()));
    set<RV*>::iterator cs2_it;
    for (cs2_it = Cunion.begin();cs2_it != Cunion.end(); cs2_it++) {
      RV* rv = (*cs2_it);
      set<RV*> tmp;
      set_intersection(rv->neighbors.begin(),rv->neighbors.end(),
		       Ps.begin(),Ps.end(),
		       inserter(tmp,tmp.end()));
      if (tmp.size() == 0)
	continue;
      set <RV*> rvPsinter;
      set_difference(tmp.begin(),tmp.end(),
		     Cs1.begin(),Cs1.end(),
		     inserter(rvPsinter,rvPsinter.end()));
      if (rvPsinter.size() > 0) {
	// ok, then there is stuff that needs to be augmented.
	set<RV*>::iterator intr_it;
	for (intr_it = rvPsinter.begin(); intr_it != rvPsinter.end(); intr_it ++) {
	  RV* intr_rv = (*intr_it);
	  // compute the position relative to the beginning of Cs1 and added it
	  // to the augmentation.
	  RVInfo::rvParent pp(intr_rv->name(),intr_rv->frame()-firstFrameCs1);
	  augmentation.insert(pp);
	  infoMsg(Max,"Ps<->Cu2: Adding %s(%d) to augmentation\n",pp.first.c_str(),pp.second);
	  // add it to Cs1 so that we don't add the same augmentation
	  // (but a different STL object) to 'augmentation' again. Add the absolute
	  // position.
	  Cs1.insert(intr_rv);
	}
      }
    }

    // for symmetry, we add any additions to Cs2.
    augmentToAbideBySMarkov(rvs,pos,augmentation,Cs2,firstFrameCs2);
  }

}


/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::augmentToAbideBySMarkov
 *      For the set of RVs in (rvs,pos), add the augmentation
 *      given in relative terms in the 'augmentation' argument
 *      to the argument Cs, which has starting frame 'firstFrameCs'
 * 
 * Preconditions:
 *      Set 'Cs' must contain RVs that are part of 'rvs'. 
 *      Must have that firstFrameCs = argframemin (Cs)
 *
 * Postconditions:
 *      Adjustment is placed in Cs
 *
 * Side Effects:
 *      Could modify Cs if augmentation is non-empty.
 *
 * Results:
 *      returns nothing.
 *
 *-----------------------------------------------------------------------
 */
void BoundaryTriangulate::augmentToAbideBySMarkov(const vector <RV*>& rvs,
						  map < RVInfo::rvParent, unsigned >& pos,
						  const set < RVInfo::rvParent >& augmentation,
						  set<RV*>& Cs,
						  const unsigned firstFrameCs)
{
  set < RVInfo::rvParent >::iterator adj_iter;
  for (adj_iter = augmentation.begin(); adj_iter != augmentation.end(); adj_iter ++) {
    RVInfo::rvParent pp = (*adj_iter);
    pp.second += firstFrameCs;
    RV* rv = getRV(rvs,pos,pp); 
    Cs.insert(rv);
  }
}




/*
  Misc functions for weight computation.
*/

float flowMinSizeHelper(const set<RV*>& rvs) {
  return rvs.size();
}

float flowMinWeightHelper(const set<RV*>& rvs) {
  return MaxClique::computeWeight(rvs, NULL, true);
}

float flowMinNoDWeightHelper(const set<RV*>& rvs) {
  return MaxClique::computeWeight(rvs, NULL, false);
}

/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::findBestInterface()
 *
 *  An exponential-time routine to find best left (or right)
 *  interfaces and corresponding partitions. Since the operations for
 *  finding the best left and right interfaces are symmetric, the case
 *  of if we are searching for the best left interface or right
 *  interface is determined entirely based on the arguments that are
 *  passed into these routines (i.e., just take the mirror image of
 *  the arguments for the right interface).  Note that for simplicity,
 *  the names have been defined in terms of the left interface.
 *
 *  The routine is given portions of an unrolled graph, P,C1,C2,C3,E,
 *  where C1 is one chunk, C2 is M chunks, and C3 is one chunk. The
 *  routine finds the best left (or right) interface entirely within
 *  C2 by searching for the boundary with the best interface.  The
 *  Boundary may span M chunks. The algorithm starts at the boundary
 *  corresponding to the "standard" or initial left interface between
 *  C1 and C2. Nodes in the left interface are shifted left across the
 *  boundary (thereby advancing the boundary). When a node is shifted
 *  left, then a new boundary forms. Note that the routine only uses
 *  the partitions C1,C2, and C3 as P and E are not needed (and can
 *  even be empty).
 *
 *
 * For left interface:
 *   Given a the unrolled graph, P,C1,C2,C3,E, find the best left
 *   interface within C2 starting at the boundary corresponding to the
 *   "standard" or initial left interface between C1 and C2.  Note
 *   that the routine only uses C1,C2, and C3 and P and E are not
 *   needed.
 *
 * For right interface:
 *   Given an unrolled graph, P,C1,C2,C3,E, find the best right
 *   interface within C2 starting at the "standard" or initial right
 *   interface between C3 and C2.  Note that the routine only uses
 *   C1,C2, and C3 and P and E are not needed. In order to get
 *   the behavior for right interface, the routine is called by
 *   swapping the arguments for C1 and C3 (see the caller).
 *
 *
 * Preconditions:
 *     Graph must be valid (i.e., unroller should pass graph w/o
 *                                problem).
 *     Graph must be already moralized.
 *     and *must* unrolled exactly M+1 times.
 *     This means graph must be in the form P,C1,C2,C3,E
 *     where P = prologue, 
 *           C1 = first chunk
 *           C2 = 2nd section, M chunks long
 *           C3 = last chunk
 *           E =  epilogue
 *
 *     Note that P or E (but not both) could be empty (this
 *     is why we do this procedure on the unrolled-by-2 graph
 *     rather than on the unrolled-by- 0 or 1 graph).
 *
 * Postconditions:
 *     Return values have best left-interface found.
 *
 * Side Effects:
 *      None
 *
 *
 * Results:
 *    Put best left (resp. right) interface from C2 into C_l
 *    and place any additional variables from C2 to the left (resp.
 *    right) of C_l into 'left_C_l';
 *
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::findBestInterface(
 // prologue
 const set<RV*> &P,
 // first chunk, 1 chunk long
 const set<RV*> &C1,
 // second chunk, M chunks long (where the boundary is located)
 const set<RV*> &C2,
 // first chunk of C2, empty when M=1
 const set<RV*> &C2_1,
 // third chunk, 1 chunk long
 const set<RV*> &C3,
 // epilogue
 const set<RV*> &E,
 // nodes to the "left" of the left interface within C2
 set<RV*> &left_C_l,
 // the starting left interface
 set<RV*> &C_l,
 // the resulting score of the best interface
 vector<float>& best_score,
 // what should be used to judge the quality of the interface
 const vector<BoundaryHeuristic>& bnd_heur_v,
 // true if we should find best (e.g., use the exponential time optimal interface algorithm)
 const bool findBest,
 // --------------------------------------------------------------
 // The next 7 input arguments are used only with the optimal
 // interface algorithm when the IH_MIN_MAX_C_CLIQUE or
 // IH_MIN_MAX_CLIQUE heuristics are used:
 // triangulation heuristic
 const TriangulateHeuristics& tri_heur,
 // The network unrolled 1 time (TODO: change to M+S-1)
 const set<RV*>& P_u1,
 const set<RV*>& C1_u1,
 const set<RV*>& Cextra_u1,
 const set<RV*>& C2_u1,
 const set<RV*>& E_u1,
 // Mappings from C2 in the twice unrolled network to C1 and C2
 // in the once unrolled network.
 // (these next 2 should be const, but there is no "op[] const" in STL)
 map < RV*, RV* >& C2_u2_to_C1_u1,
 map < RV*, RV* >& C2_u2_to_C2_u1
 // end of input arguments (finally)
 )
{

  // left interface
  C_l.clear();

  // First, construct the basic left interface (i.e., left interface
  // of C2).

  // initial left interface of C2 consists of all nodes in C2 either that
  //   1) are also contained in union(P,C1) (i.e., contained in both C2 and union(P,C1)), or that
  //   2) have a neighbor in union(P,C1).
  // 
  // First, insert intersection(C2,union(P,C1)) == union(intersection(C2,P),intersection(C2,C1))
  set_intersection(C2.begin(),C2.end(),
		   C1.begin(),C1.end(),
		   inserter(C_l,C_l.end()));
  set_intersection(C2.begin(),C2.end(),
		   P.begin(),P.end(),
		   inserter(C_l,C_l.end()));
  // printf("LI intersection =");printRVSet(stdout,C_l); 
  // 
  // Next, if there exists any v in C2 that has a neighbor in union(P,C1)
  // and that neighbor is not already in C2, then v also goes into the left interface.
  // If, on the other hand, all of v's neighbors in union(P,C1) are
  // already in C2, then v doesn't become part of the left interface
  // (even if it does have neighbors in union(P,C1)). The reason is that
  // we don't need v to act as part of a separator any longer, since those
  // neighbors of of v that are in both union(P,C1) and C2 already act as the
  // separator, so it would be unnecessary to add such a v.
  set<RV*>::iterator c2_iter;
  for (c2_iter = C2.begin(); c2_iter != C2.end(); c2_iter ++) {
    RV *cur_rv = (*c2_iter);
    set<RV*> res;
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     C1.begin(),C1.end(),
		     inserter(res,res.end()));
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     P.begin(),P.end(),
		     inserter(res,res.end()));
    if (res.size() > 0) {
      // ok, we have neighbors in union(P,C1) that are in res, now check to
      // sure that res is not already in C2.
      set<RV*> tmp;
      set_intersection(res.begin(),res.end(),
		       C2.begin(),C2.end(),
		       inserter(tmp,tmp.end()));
      if (tmp.size() != res.size()) {
	// then at least one of cur_rv's neighbors in union(P,C1) is not in C2,
	// so we add it.
	C_l.insert(cur_rv);
      }
    }
  }


  // Like above, final left interface of C2 consists of all nodes in
  // C2 either that 1) have a neighbor in union(C3,E) or that are
  // contained in both C2 and union(C3,E). These are the nodes that
  // the l-interface might contain, but we can't go beyond.

  // left interface
  set <RV*> finalLI;
  set_intersection(C2.begin(),C2.end(),
		   C3.begin(),C3.end(),
		   inserter(finalLI,finalLI.end()));
  set_intersection(C2.begin(),C2.end(),
		   E.begin(),E.end(),
		   inserter(finalLI,finalLI.end()));
  // next, if any v \in C2 has a neighbor in union(C3,E) that is not
  // already contained within C2, it also goes in (see above expanded comment).
  for (c2_iter = C2.begin(); c2_iter != C2.end(); c2_iter ++) {
    RV *cur_rv = (*c2_iter);
    set<RV*> res;
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     C3.begin(),C3.end(),
		     inserter(res,res.end()));
    set_intersection(cur_rv->neighbors.begin(),cur_rv->neighbors.end(),
		     E.begin(),E.end(),
		     inserter(res,res.end()));
    if (res.size() > 0) {
      // ok, we have neighbors in union(C3,E) that are in res, now check to
      // sure that res is not already in C2.
      set<RV*> tmp;
      set_intersection(res.begin(),res.end(),
		       C2.begin(),C2.end(),
		       inserter(tmp,tmp.end()));
      if (tmp.size() != res.size())
	finalLI.insert(cur_rv);
    }
  }

  
  if (findBest && (bnd_heur_v.size() == 1) &&
      ((bnd_heur_v[0] == IH_MIN_SIZE) 
       || (bnd_heur_v[0] == IH_MIN_WEIGHT_NO_D)))
    {      
      // only do min size and min weight no d since those are the ones
      // that can be evaluated as a sum of RV costs.

      float (*cw)(const set<RV*>&) = NULL;
      if (bnd_heur_v[0] == IH_MIN_WEIGHT_NO_D)
	cw = &flowMinNoDWeightHelper;
      else if (bnd_heur_v[0] == IH_MIN_WEIGHT)
	cw = &flowMinWeightHelper;
      else
	cw = &flowMinSizeHelper;

      if (message(Tiny)) {
	printf("Initial boundary has size %ld: ",(unsigned long)C_l.size());
	printRVSet(stdout,C_l);
      }

      // find best boundary by network flow algorithm.
      GMTK2Network network(cw, C_l, C2, finalLI);
      double flow = network.findMaxFlow();
      std::set<RV*> bestC_l; 
      std::set<RV*> leftBestC_l; 
      network.findBoundary(bestC_l, leftBestC_l);
      C_l = bestC_l;
      left_C_l = leftBestC_l; 
      
      if (message(Tiny)) {
	printf("Best flow found is %f\n", flow); 
	printf("Best boundary found has size %ld: ",(unsigned long)C_l.size());
	printRVSet(stdout,C_l);
	if (message(Med)) {
	  printf("Best (left of boundary) found has size %ld and is:",(unsigned long)left_C_l.size());
	  printRVSet(stdout,left_C_l);
	}
      }
      return; 
      
      // TODO: check of all heuristics in bnd_heur_v are
      // submodular, then prioritized ordering of them
      // is also submodular, and pass this prioritized
      // ordering down to the maxflow/submodular routine
      // rather than just passing bnd_heur_v[0] down.

      // TODO: then call max-flow or submodular optimization routine
      // to find best boundary. We have that C_l are the nodes connected
      // to the source, and finalLI are the nodes connected to the sync.
      // The routine should ignore all variables except for those
      // in C2. I.e., if any variables in C2 have neighbors out of
      // C2, they should be both left alone and ignored. The routine
      // should not change any neighbors of any variables in C2.
      // place result in the ref. argument best_C_l
      // set<RV*> best_C_l;
      // findBestInterfaceMaxFlowSubmodular(bnd_heur_v[0],C_l,finalLI,C2,bestC_l);
      // C_l = best_C_l;
      // return;

    }

  // Still here? Call exponential routine.


  // Since the interface is currently the first one, 
  // the set left of the left interface within C2 is now empty.
  left_C_l.clear();

  // Note that the partition "boundary" is the border/line that cuts
  // the edges connecting nodes C_l and nodes left_C_l.

  // how good is this interface.
  interfaceScore(bnd_heur_v,C_l,left_C_l,
		 tri_heur,
		 P_u1,C1_u1,Cextra_u1,C2_u1,E_u1,
		 C2_u2_to_C1_u1,C2_u2_to_C2_u1,
		 best_score);

  if (message(Tiny)) {
    printf("  Initial interface size = %ld\n",(unsigned long)C_l.size());
    printf("  Initial interface score =");
    for (unsigned i=0;i<best_score.size();i++)
      printf(" %f ",best_score[i]);
    printf("\n");
    // Note that this next print always prints the string "left_C_l"
    // but if we are running the right interface version, it should
    // print the string "right_C_l" instead. The algorithm however
    // does not know if it is running left or right interface.
    infoMsg(Med,"  Initial left_C_l size = %d\n",left_C_l.size());
    {
      printf("  Initial Interface:");
      set<RV*>::iterator i;    
      for (i=C_l.begin();i!=C_l.end();i++) {
	printf(" %s(%d)",
	       (*i)->name().c_str(),
	       (*i)->frame());
	     
      }
      printf("\n");
    }
    if (message(Med)) {
      infoMsg(Med,"  Final interface: "); 
      printRVSet(stdout,finalLI);
    }
  }


  // start exponential recursion to find the truly best interface.
  if (findBest) {
    // best ones found so far
    set<RV*> best_left_C_l = left_C_l;
    set<RV*> best_C_l = C_l;
    set< set<RV*> > setset;
    // call recursive routine.
    boundaryRecursionDepth = 0;
    findBestInterfaceRecurse(left_C_l,
			     C_l,
			     C2,
			     C2_1,
			     finalLI,
			     setset,
			     best_left_C_l,
			     best_C_l,
			     best_score,bnd_heur_v,
			     // 
			     tri_heur,
			     P_u1,C1_u1,Cextra_u1,C2_u1,E_u1,
			     C2_u2_to_C1_u1,C2_u2_to_C2_u1);
    if (message(Tiny)) {
      printf("  Size of best interface = %ld\n",(unsigned long)best_C_l.size());
      printf("  Score of best interface =");
      for (unsigned i=0;i<best_score.size();i++)
	printf(" %f ",best_score[i]);
      printf("\n");
      infoMsg(Med,"  Size of best_left_C_l = %d\n",best_left_C_l.size());
      {
	printf("  Best interface nodes include:");
	set<RV*>::iterator i;    
	for (i=best_C_l.begin();i!=best_C_l.end();i++) {
	  printf(" %s(%d)",
		 (*i)->name().c_str(),
		 (*i)->frame());
	}
	printf("\n");

      }
    }
    left_C_l = best_left_C_l;
    C_l = best_C_l;
  }
}



/*-
 *-----------------------------------------------------------------------
 * BoundaryTriangulate::findBestInterfaceRecurse() 
 *    Recursive helper function for the * first version of
 *    findBestInterface(). See that routine above for * documentation.
 *
 * Preconditions:
 *
 * Postconditions:
 *
 * Side Effects:
 *
 * Results:
 *
 *-----------------------------------------------------------------------
 */
void
BoundaryTriangulate::
findBestInterfaceRecurse(
  const set<RV*> &left_C_l,
  const set<RV*> &C_l,
  const set<RV*> &C2,
  const set<RV*> &C2_1,
  const set<RV*> &finalLI,
  set< set<RV*> >& setset,
  set<RV*> &best_left_C_l,
  set<RV*> &best_C_l,
  vector<float>& best_score,
  const vector<BoundaryHeuristic>& bnd_heur_v,
  // --------------------------------------------------------------
  // The next 7 input arguments are used only with the optimal
  // interface algorithm when the IH_MIN_MAX_C_CLIQUE or
  // IH_MIN_MAX_CLIQUE heuristics are used:
  const TriangulateHeuristics& tri_heur,
  const set<RV*>& P_u1,
  const set<RV*>& C1_u1,
  const set<RV*>& Cextra_u1,
  const set<RV*>& C2_u1,
  const set<RV*>& E_u1,
  // these next 2 should be const, but there is no "op[] const"
  map < RV*, RV* >& C2_u2_to_C1_u1,
  map < RV*, RV* >& C2_u2_to_C2_u1
)
{
  if (timer && timer->Expired()) {
    infoMsg(IM::Low, "Exiting boundary algorithm because time has expired\n");
    return;
  }
  boundaryRecursionDepth++;
  set<RV*>::iterator v;  // vertex
  // consider all v in the current C_l as candidates to be moved
  // left. Essentially, we advance the "partition boundary" to the right
  // by taking nodes that are right of the boundary and moving those
  // nodes (one by one) to the left of the boundary. This is done
  // recursively, and memoization is employed to ensure that we don't
  // redundantly explore the same partition boundary.
  // In all cases, 
  //  1) the boundary is a set of edges (and is not explicitly
  //     represented in the code).
  //  2) C_l are the nodes adjacent and to and directly to the right of the current boundary 
  //  3) left_C_l are the nodes to the left of the current boundary (i.e., they
  //     are former interface nodes that have been shifted left accross the boundary).

  // old comment:
  // All nodes in C1 must be to the left of the boundary (ensured by
  // start condition and by the nature of the algoritm).  All nodes to
  // the right of the boundary must still be in C2, or otherwise
  // we would try to produce a boundary with > M chunks. This could
  // lead to invalid networks since the left interface structure in E
  // might not be the same as the left interface structure in C (and this
  // is the reason for unrolling u2 by M+1, to get an extra chunk at the
  // beginning and end of of the chunks in which we do a boundary search).
  // This condition is ensured by "condition ***" below.

  // If C2 consists of multiple chunks (say C2_1, C2_2, etc.) then we
  // should never have that all of C2_1 lies completely to the left of
  // the boundary, since this would be redundant (i.e., a boundary
  // between C1 and C2_1 would be the same edges shifted in time as
  // the boundary between C2_1 and C2_2). This is ensured by
  // "condition ###" below where further comments on this point are
  // given.
  for (v = C_l.begin(); v != C_l.end(); v ++) {
    // TODO: 
    //      Rather than "for all nodes in C_l", we could do a random
    //      subset of nodes in C_l to speed this up if it takes too
    //      long for certain graphs.  Alternatively, a greedy strategy
    //      could be employed where rather than recursing for each v,
    //      we could first choose the best v and then recurse on that.
    //      But note that this is only run once per graph so it will
    //      be beneficial to do this since its cost would probably be
    //      ammortized over the many runs of inference with the graph.

    // continue with probabilty 'boundaryTraverseFraction'
    if (!rnd.coin(boundaryTraverseFraction))
      continue;

    // Condition ***: if v is in finalLI, then continue since if v was
    // moved left, we would end up with a boundary > M chunks. This
    // would be invalid since there would be a node left of C_l that
    // connects directly to the right of C_l. 
    if (finalLI.find((*v)) != finalLI.end())
      continue;

#if 0
    set_intersection((*v)->neighbors.begin(),
		     (*v)->neighbors.end(),
		     C3.begin(),C3.end(),
		     inserter(res, res.end()));
    if (res.size() != 0)
      continue;
#endif

    // Then it is ok to remove v from C_l and move v to the left.
    // take v from C_l and place it in left_C_l
    set<RV*> next_left_C_l = left_C_l;
    next_left_C_l.insert((*v));

    // Only do this check if C2_1 is non-empty. If it is empty, we
    // assume the check is not needed (say because C2 is itself only
    // one chunk wide).
    if (C2_1.size() > 0) {
      // Condition ###: next check to make sure that we haven't used
      // up all the nodes in C2_1, because if we did we would be in
      // the (M - 1) case (i.e., the only other possible boundaries
      // would span M-1 chunks, skipping the first one). Any
      // boundaries that we return thus will always have a left
      // interface with at least one node in the first chunk of C2, which
      // we call C2_1. Note that it is possible to return a boundary
      // that spans fewer than M chunks (which means that the optimal
      // interface would have been found for a smaller M).

      // We ensure this condition by making sure that C2_1 is not a
      // proper subset of (left_C_l U {v}) = next_left_C_l.

      count_iterator<set <RV*> > tmp;      
      set_difference(C2_1.begin(),C2_1.end(),
		     next_left_C_l.begin(),next_left_C_l.end(),
		     tmp);
      if (tmp.count() == 0)
	continue;
    }

    if (message(VerbosityLevels(High))) {
      printf("  Recursion depth = %d\n",boundaryRecursionDepth);
      printf("  Moving node %s(%d) over boundary\n",
	     (*v)->name().c_str(),(*v)->frame());
    }

#if 0
    // Next, create the new left interface, next_C_l by adding all
    // neighbors of v that are in C3 U C2\next_left_C_l to an
    // initially empty next_C_l.
#endif


    // Next, create the new left interface, next_C_l by adding all
    // neighbors of v that are in C3 U C2\next_left_C_l to an
    // initially empty next_C_l.
    set<RV*> next_C_l;
    set<RV*> tmp;

#if 0
    // TODO: remove this next check and fix comments above, since the
    // check is redundant now.
    // add neighbors of v that are in C3
    set_intersection((*v)->neighbors.begin(),
		     (*v)->neighbors.end(),
		     C3.begin(),C3.end(),
		     inserter(tmp,tmp.end()));
    assert ( tmp.size() == 0 );
#endif

    // add neighbors of v that are in C2
    set_intersection((*v)->neighbors.begin(),
		     (*v)->neighbors.end(),
		     C2.begin(),C2.end(),
		     inserter(tmp,tmp.end()));

    // remove nodes that are in next_left_C_l
    set<RV*> res;
    set_difference(tmp.begin(),tmp.end(),
		   next_left_C_l.begin(),next_left_C_l.end(),
		   inserter(res,res.end()));
    // add this to the previous left interface
    set_union(res.begin(),res.end(),
	      C_l.begin(),C_l.end(),
	      inserter(next_C_l,next_C_l.end()));
    // and lastly, remove v from the new left interface since that was
    // added just above.
    next_C_l.erase((*v));

    // check if we've seen this left interface already.
    if (!noBoundaryMemoize) {
      if (setset.find(next_C_l) != setset.end()) {
	if (message(VerbosityLevels(High))) {
	  printf("  Continuing: found existing interface:");
	  set<RV*>::iterator i;    
	  for (i=next_C_l.begin();i!=next_C_l.end();i++) {
	    printf(" %s(%d)",
		   (*i)->name().c_str(),
		   (*i)->frame());
	  }	
	  printf("\n");
	}
	continue; // check if memoized, if so, no need to go further.
      }

      // memoize
      setset.insert(next_C_l);
    }

    // score the new left interface
    vector<float> next_score;


    interfaceScore(bnd_heur_v,next_C_l,next_left_C_l,
		   tri_heur,
		   P_u1,C1_u1,Cextra_u1,C2_u1,E_u1,
		   C2_u2_to_C1_u1,C2_u2_to_C2_u1,
		   next_score);
    // check size of candiate interface, and keep a copy of it if it
    // is strictly less then the one we have seen so far.
    if (next_score < best_score) {
      best_left_C_l = next_left_C_l;
      best_C_l = next_C_l;
      best_score = next_score;
    } 

    // recurse
    findBestInterfaceRecurse(next_left_C_l,
			     next_C_l,
			     C2,C2_1,finalLI,
			     setset,
			     best_left_C_l,best_C_l,best_score,
			     bnd_heur_v,
			     tri_heur,
			     P_u1,C1_u1,Cextra_u1,C2_u1,E_u1,
			     C2_u2_to_C1_u1,C2_u2_to_C2_u1);

  }

  boundaryRecursionDepth--;
}


