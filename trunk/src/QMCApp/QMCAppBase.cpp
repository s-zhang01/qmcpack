//////////////////////////////////////////////////////////////////
// (c) Copyright 2003- by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   Jeongnim Kim
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
#include "Configuration.h"
#include "QMCApp/QMCAppBase.h"
namespace qmcplusplus {

  QMCAppBase::QMCAppBase(int argc, char** argv)
  {
  }

  QMCAppBase::~QMCAppBase() {
    while(!XmlDocStack.empty()) { popDocument(); }
    DEBUGMSG("QMCAppBase::~QMCAppBase")
  }

  bool QMCAppBase::pushDocument(const string& infile) {
    Libxml2Document* adoc= new Libxml2Document();
    bool success = adoc->parse(infile);

    if(success) {
      XmlDocStack.push(adoc);
    }  else {
      app_error() << "File " << infile << " is invalid" << endl;
      delete adoc;
    }
    return success;
  }

  void QMCAppBase::popDocument() {
    if(!XmlDocStack.empty()) { //Check if the stack is empty
      Libxml2Document* adoc=XmlDocStack.top();
      delete adoc;
      XmlDocStack.pop();
    }
  }

  /** parse an input file
   * @param infile name of an input file
   * @return true, if the document is a valid xml file.
   *
   * The data members m_doc and m_root point to the "current" document and 
   * root element.
   */
  bool QMCAppBase::parse(const string& infile) {

    return pushDocument(infile);
  }

  void QMCAppBase::saveXml() {

    if(!XmlDocStack.empty()) {
      string newxml(myProject.CurrentRoot());
      //myProject.PreviousRoot(newxml);
      //myProject.rewind();
      newxml.append(".cont.xml");
      app_log() << "A new xml input file : " << newxml << endl;
      XmlDocStack.top()->dump(newxml);
    }
  }
}
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/
