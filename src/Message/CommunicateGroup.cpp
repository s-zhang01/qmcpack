//////////////////////////////////////////////////////////////////
// (c) Copyright 1998-2002, 2003- by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   National Center for Supercomputing Applications &
//   Materials Computation Center
//   University of Illinois, Urbana-Champaign
//   Urbana, IL 61801
//   e-mail: jnkim@ncsa.uiuc.edu
//   Tel:    217-244-6319 (NCSA) 217-333-3324 (MCC)
//
// Supported by
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//   Department of Physics, Ohio State University
//   Ohio Supercomputer Center
//////////////////////////////////////////////////////////////////
// -*- C++ -*-

#include "Message/CommunicateGroup.h"
#include "Message/TagMaker.h"

CommunicateGroup::CommunicateGroup(Communicate& acomm, int ndiv):
  Communicate(acomm.split(ndiv)), parent(acomm)
{
}

/***************************************************************************
 * $RCSfile$   $Author: jmcminis $
 * $Revision: 5794 $   $Date: 2013-04-25 20:14:53 -0400 (Thu, 25 Apr 2013) $
 * $Id: CommunicateGroup.cpp 5794 2013-04-26 00:14:53Z jmcminis $
 ***************************************************************************/
