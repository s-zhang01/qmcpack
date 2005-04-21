#include "QMCTools/GaussianFCHKParser.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <set>
#include <map>

using namespace std;

GaussianFCHKParser::GaussianFCHKParser() {
  basisName = "Gaussian-G2";
  Normalized = "no";
}

GaussianFCHKParser::GaussianFCHKParser(int argc, char** argv): 
  QMCGaussianParserBase(argc,argv) {
  basisName = "Gaussian-G2";
  Normalized = "no";
}

void GaussianFCHKParser::parse(const std::string& fname) {

  std::ifstream fin(fname.c_str());

  getwords(currentWords,fin); //1
  Title = currentWords[0];

  getwords(currentWords,fin);//2  SP RHF Gen
  if(currentWords[1]=="RHF") {
    SpinRestricted=true;
  } else {
    SpinRestricted=false;
  }

  getwords(currentWords,fin);//3  Number of atoms
  NumberOfAtoms = atoi(currentWords.back().c_str());

  getwords(currentWords,fin); //4 Charge
  getwords(currentWords,fin); //5 Multiplicity

  getwords(currentWords,fin); //6 Number of electrons
  NumberOfEls=atoi(currentWords.back().c_str());

  getwords(currentWords,fin); //7 Number of alpha electrons
  int nup=atoi(currentWords.back().c_str());
  getwords(currentWords,fin); //8 Number of beta electrons 
  int ndown=atoi(currentWords.back().c_str());

  getwords(currentWords,fin); //9 Number of basis functions
  SizeOfBasisSet=atoi(currentWords.back().c_str());
  getwords(currentWords,fin); //10 Number of independant functions 
  getwords(currentWords,fin); //11 Number of contracted shells
  int ng=atoi(currentWords.back().c_str());

  //allocate everything here
  R.resize(NumberOfAtoms);
  GroupID.resize(NumberOfAtoms);
  Qv.resize(NumberOfAtoms);

  gBound.resize(NumberOfAtoms+1);
  gShell.resize(ng); 
  gNumber.resize(ng);
  gExp.resize(ng); 
  gC0.resize(ng); 
  gC1.resize(ng);

  search(fin, "Atomic numbers");//search for Atomic numbers
  getGeometry(fin);

  search(fin, "Shell types");
  getGaussianCenters(fin);

  search(fin, "Alpha MO");
  int nstates=SizeOfBasisSet;
  if(SpinRestricted)
    EigVec.resize(SizeOfBasisSet*SizeOfBasisSet);
  else {
    nstates*=2;
    EigVec.resize(2*SizeOfBasisSet*SizeOfBasisSet);
  }
  getValues(fin,EigVec.begin(), EigVec.end());
}

void GaussianFCHKParser::getGeometry(std::istream& is) {
  //atomic numbers
  getValues(is,GroupID.begin(),GroupID.end());
  search(is,"Nuclear");
  getValues(is,Qv.begin(),Qv.end());
  search(is,"coordinates");
  getValues(is,R.begin(),R.end());
}

void GaussianFCHKParser::getGaussianCenters(std::istream& is) {

  //map between Gaussian to Casino Shell notation
  std::map<int,int> gsMap;
  gsMap[0] =1; //s
  gsMap[-1]=2; //sp
  gsMap[1] =3; //p
  gsMap[-2]=4; //d

  vector<int> n(gShell.size()), dn(NumberOfAtoms,0);

  getValues(is,n.begin(), n.end());
  for(int i=0; i<n.size(); i++) gShell[i]=gsMap[n[i]];

  search(is, "Number");
  getValues(is,gNumber.begin(), gNumber.end());

  search(is, "Shell");
  getValues(is,n.begin(), n.end());
  for(int i=0; i<n.size(); i++) dn[n[i]-1]+=1;

  gBound[0]=0;
  for(int i=0; i<NumberOfAtoms; i++) {
    gBound[i+1]=gBound[i]+dn[i];
  }

  search(is, "Primitive");
  getValues(is,gExp.begin(), gExp.end());

  search(is, "Contraction");
  getValues(is,gC0.begin(), gC0.end());

  search(is, "P(S=P)");
  getValues(is,gC1.begin(), gC1.end());
}

