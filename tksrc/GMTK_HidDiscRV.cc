/*
 * GMTK_HidDiscRV.cc
 *
 * Observed discrete random variable.
 * 
 *
 * Written by Jeff Bilmes <bilmes@ee.washington.edu>
 *  $Header$
 *
 * Copyright (C) 2001 Jeff Bilmes
 * Licensed under the Open Software License version 3.0
 * See COPYING or http://opensource.org/licenses/OSL-3.0
 *
 *
 *
 * The top level GMTK random variable object for the RV class hierarchy.
 *
 *
 *
 */



#include "general.h"
#if HAVE_CONFIG_H
#include <config.h>
#endif
#if HAVE_HG_H
#include "hgstamp.h"
#endif
VCID(HGID)


#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <string.h>

#include "GMTK_HidDiscRV.h"


/*-
 *-----------------------------------------------------------------------
 * printSelf()
 *      prints a one-line summary of the detailed information about this RV.
 *
 * Preconditions:
 *      none
 *
 * Postconditions:
 *      none
 *
 * Side Effects:
 *      none
 *
 * Results:
 *      self is printed.
 *
 *-----------------------------------------------------------------------
 */
void HidDiscRV::printSelf(FILE *f,bool nl)
{
  printNameFrameValue(f,false);
  fprintf(f," hidden discrete cardinality = %d%s",cardinality,nls(nl));
}



/*-
 *-----------------------------------------------------------------------
 * printSelfVerbose()
 *      prints a multi-line verbose description of this RV.
 *
 * Preconditions:
 *      none
 *
 * Postconditions:
 *      none
 *
 * Side Effects:
 *      none
 *
 * Results:
 *      self is printed.
 *
 *-----------------------------------------------------------------------
 */
void HidDiscRV::printSelfVerbose(FILE *f)
{
  fprintf(f,"Hidden Discrete Random variable:\n");
  printNameFrameValue(f,true);
  fprintf(f,"From line %d in file %s\n",rv_info.fileLineNumber,rv_info.rvFileName.c_str());
  fprintf(f,"RV has cardinality = %d\n",cardinality);
}



/*-
 *-----------------------------------------------------------------------
 * cloneRVShell()
 *      clones a shell of the current random variable (see GMTK_RV.h for docs)
 *
 * Preconditions:
 *      RV must be instantiated and with parameters (i.e., what lives in the template RVs).
 *
 * Postconditions:
 *      none
 *
 * Side Effects:
 *      none
 *
 * Results:
 *      self is printed.
 *
 *-----------------------------------------------------------------------
 */
HidDiscRV* HidDiscRV::cloneRVShell()
{
  HidDiscRV*rv = (HidDiscRV*)DiscRV::cloneRVShell();
  return rv;
}

