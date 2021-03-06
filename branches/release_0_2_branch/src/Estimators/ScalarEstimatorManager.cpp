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
//////////////////////////////////////////////////////////////////
// -*- C++ -*-
#include "Particle/MCWalkerConfiguration.h"
#include "Estimators/ScalarEstimatorManager.h"
#include "QMCHamiltonians/QMCHamiltonian.h"
#include "Message/Communicate.h"
#include "Message/CommOperators.h"
#include "Estimators/LocalEnergyEstimator.h"
#include "Estimators/PolarizationEstimator.h"

using namespace qmcplusplus;

ScalarEstimatorManager::ScalarEstimatorManager(QMCHamiltonian& h): 
 FileManager(true), CollectSum(false),
 Period(1000), NodeWeight(1.0), H(h), RootName("estimator"), 
  OutStream(0) { 
  
  }

ScalarEstimatorManager::~ScalarEstimatorManager(){ 
  Estimators.erase(Estimators.begin(), Estimators.end());
  if(OutStream) delete OutStream;
}

int
ScalarEstimatorManager::add(EstimatorType* newestimator, const string& aname) { 

  std::map<string,int>::iterator it = EstimatorMap.find(aname);
  if(it == EstimatorMap.end()) {
    int n =  Estimators.size();
    Estimators.push_back(newestimator);
    EstimatorMap[aname] = n;
    //newestimator->add2Record(BlockAverages);
    return n;
  } else {
    cout << "Already added estimator " << aname << endl;
    return (*it).second;
  }
}

/** reset the internal data of all the estimators for new averages
 */
void 
ScalarEstimatorManager::reset() {
  MyData=0.0;
  for(int i=0; i< Estimators.size(); i++) Estimators[i]->reset();
}

/** accumulate data for all the estimators
 */
void 
ScalarEstimatorManager::accumulate(const MCWalkerConfiguration& W) {

  RealType wgt_sum=0;
  MCWalkerConfiguration::const_iterator it(W.begin()),it_end(W.end());
  while(it != it_end) {
    RealType wgt = (*it)->Weight;
    wgt_sum+= wgt;
    for(int i=0; i< Estimators.size(); i++) Estimators[i]->accumulate(**it,wgt);
    ++it;
  }

  MyData[WEIGHT_INDEX]+=wgt_sum;
}

/**  compute the averages for all the estimators and reset
 */
void ScalarEstimatorManager::flush(){

  if(CollectSum) gsum(MyData,0);

  RealType wgtinv = 1.0/MyData[WEIGHT_INDEX];
  for(int i=0; i<Estimators.size(); i++) 
    Estimators[i]->report(BlockAverages,wgtinv);

  BlockAverages[MyIndex[WEIGHT_INDEX]]=MyData[WEIGHT_INDEX];
  BlockAverages[MyIndex[BLOCK_CPU_INDEX]] = MyData[BLOCK_CPU_INDEX]*NodeWeight;
  BlockAverages[MyIndex[ACCEPT_RATIO_INDEX]] = MyData[ACCEPT_RATIO_INDEX]*NodeWeight;

  MyData=0.0;
}


/** print the averages for all the estimators to a file
 * @param iter the interval 
 */
void ScalarEstimatorManager::report(int iter){
  if(FileManager) {
    (*OutStream) << setw(10) << iter;
    for(int i=0; i<BlockAverages.size();i++)
      (*OutStream) << setw(16) << BlockAverages[i];
    (*OutStream) << endl;
  }
}

/** combines the functionality of flush and report
 * @param iter the interval
 */
void ScalarEstimatorManager::flushreport(int iter){

  if(CollectSum) gsum(MyData,0);
  RealType wgtinv = 1.0/MyData[WEIGHT_INDEX];
  for(int i=0; i<Estimators.size(); i++)  Estimators[i]->report(BlockAverages,wgtinv);

  BlockAverages[MyIndex[WEIGHT_INDEX]]=MyData[WEIGHT_INDEX];
  BlockAverages[MyIndex[BLOCK_CPU_INDEX]] = MyData[BLOCK_CPU_INDEX]*NodeWeight;
  BlockAverages[MyIndex[ACCEPT_RATIO_INDEX]] = MyData[ACCEPT_RATIO_INDEX]*NodeWeight;
  MyData=0.0;

  if(FileManager) {
    (*OutStream) << setw(10) << iter;
    for(int i=0; i<BlockAverages.size();i++) (*OutStream) << setw(16) << BlockAverages[i];
    (*OutStream) << endl;
  }
}

/** set CollectSum
 * @param collect if true, global sum is done over the values
 *
 * FileManager is set when collect is true so that only the first node writes.
 */
void ScalarEstimatorManager::setCollectionMode(bool collect) {
  CollectSum=collect;
  for(int i=0; i< Estimators.size(); i++) Estimators[i]->CollectSum = collect;
  if(collect) {
    FileManager = OHMMS::Controller->master();
    NodeWeight = 1.0/static_cast<RealType>(OHMMS::Controller->ncontexts());
  } else {
    FileManager = true;
    NodeWeight = 1.0;
  }
}


void 
ScalarEstimatorManager::resetReportSettings(const string& aname, bool append) {

  //at least have local energy
  if(Estimators.empty()) {
    add(new LocalEnergyEstimator<RealType>(H),"elocal");
  } 

  //update the weight index
  for(int i=0; i<Estimators.size(); i++) 
    Estimators[i]->add2Record(BlockAverages);

  MyIndex[WEIGHT_INDEX] = BlockAverages.add("WeightSum");
  MyIndex[BLOCK_CPU_INDEX] = BlockAverages.add("BlockCPU");
  MyIndex[ACCEPT_RATIO_INDEX] = BlockAverages.add("AcceptRatio");

  RootName = aname;

  if(FileManager) {
    string fname(aname);
    fname.append(".scalar.dat");
    if(OutStream) {delete OutStream; OutStream=0;}

    if(append) 
      OutStream = new ofstream(fname.c_str(), ios::app);
    else
      OutStream = new ofstream(fname.c_str());

    OutStream->setf(ios::scientific, ios::floatfield);
    OutStream->setf(ios::left,ios::adjustfield);
  }

  BlockAverages.setValues(0.0);
}

/**  print the header to the output file
 */
void 
ScalarEstimatorManager::reportHeader(bool append) {
  if(FileManager) {
    if(!append)  {
      *OutStream << "#    index     ";
      for(int i=0; i<BlockAverages.size(); i++) 
        (*OutStream) << setw(16) << BlockAverages.Name[i];
      (*OutStream) << endl;
    }
    OutStream->setf(ios::right,ios::adjustfield);
  }
}

/** closes the stream to the output file
 */
void 
ScalarEstimatorManager::finalize() {
  if(OutStream) delete OutStream;
  OutStream = 0;
}


ScalarEstimatorManager::EstimatorType* 
ScalarEstimatorManager::getEstimator(const string& a) {
  std::map<string,int>::iterator it = EstimatorMap.find(a);
  if(it == EstimatorMap.end()) {
    return 0;
  } else {
    return Estimators[(*it).second];
  }
}

bool ScalarEstimatorManager::put(xmlNodePtr cur) {

  vector<string> extra;

  cur = cur->children;

  //count the number of Hamiltonian copies
  int nsystem=0;
  while(cur != NULL) {
    string cname((const char*)(cur->name));
    if(cname == "qmcsystem") {
      nsystem++;
    } else if(cname == "estimator") {
      xmlChar* att=xmlGetProp(cur,(const xmlChar*)"name");
      if(att) {
	string aname((const char*)att);
	if(aname == "LocalEnergy") {
          //att=xmlGetProp(cur,(const xmlChar*)"size");
	  //int ncopy(1);
          //if(att) {ncopy=atoi((const char*)att);}
          //add(new LocalEnergyEstimator<RealType>(H,ncopy),"elocal");
          add(new LocalEnergyEstimator<RealType>(H),"elocal");
	} else { 
	  extra.push_back(cname);
	}
      }
    } 
    cur = cur->next;
  }

  if(Estimators.empty()) {
    //if(nsystem == 0) nsystem=1;
    add(new LocalEnergyEstimator<RealType>(H),"elocal");
  } 

  for(int i=0; i<extra.size(); i++) {
    if(extra[i] == "Polarization"){
      add(new PolarizationEstimator<RealType>(),extra[i]);
    }
  }

  //add extra
  return true;
}

/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/
