//////////////////////////////////////////////////////////////////
// (c) Copyright 1998- by Jeongnim Kim
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
#ifndef OHMMS_CRYSTALLATTICE_H
#define OHMMS_CRYSTALLATTICE_H
#include <limits>
#include <cmath>
#include "OhmmsPETE/TinyVector.h"
#include "OhmmsPETE/Tensor.h"
#include "Lattice/ParticleBConds.h"
#include "Lattice/LatticeOperations.h"

using namespace std;

/**@file CrystalLattice.h
 *@brief Declaration of CrystalLattice<T,D>
 */
#ifndef TWOPI
#ifndef M_PI
#define TWOPI 6.3661977236758134308E-1
#else
#define TWOPI 2*M_PI
#endif /* M_PI */
#endif /* TWOPI */

namespace APPNAMESPACE {
/** class to assist copy and unit conversion operations on position vectors
 */
struct PosUnit {

  /** enumeraton for the unit of position types.
   */
  enum {CartesianUnit=0,/*!< indicates that the values are in Cartesian units*/
        LatticeUnit/*!< indicates that the values are in Lattice units*/
       };
};


/** a class that defines a supercell in D-dimensional Euclean space.
 *
 *CrystalLattice handles the physical properties of a supercell, such
 *as lattice vectors, reciprocal vectors and metric tensors and provides
 *interfaces to access the lattice properties and convert units of
 *position vectors or a single-particle position from Cartesian to
 *Lattice Unit vice versa.
 *
 *The indices for R, G and D are chosen to perform
 *expression template operations with variable-cell algorithms.
 *
 */
template<class T, unsigned D, bool ORTHO=false>
struct CrystalLattice{

  ///enumeration for the dimension of the lattice
  enum {DIM = D};
  //@{
  ///the type of scalar
  typedef T                            Scalar_t;
  ///the type of a D-dimensional position vector 
  typedef TinyVector<T,D>              SingleParticlePos_t;
  ///the type of a D-dimensional index vector 
  typedef TinyVector<int,D>            SingleParticleIndex_t;
  ///the type of a D-dimensional Tensor
  typedef Tensor<T,D>                  Tensor_t;
  //@}

  ///supercell enumeration
  int SuperCellEnum;
  ///The boundary condition in each direction.
  TinyVector<int,D> BoxBConds;
  //@{ 
  /**@brief Physcial properties of a supercell*/
  /// Volume of a supercell
  T  Volume; 
  ///Real-space unit vectors. R(i,j) i=vector and j=x,y,z
  Tensor_t R;
  ///Reciprocal unit vectors. G(j,i) i=vector and j=x,y,z
  Tensor_t G;
  ///Metric tensor
  Tensor_t M;
  ///Metric tensor for G vectors
  Tensor_t Mg;

  /**@brief Real-space unit vectors. 
   *
   *Introduced to efficiently return one vector at a time.
   *Rv[i] is D-dim vector of the ith direction. 
   */
  TinyVector<SingleParticlePos_t,D> Rv;
  /**@brief Reciprocal unit vectors. 
   *
   *Introduced to efficiently return one vector at a time.
   *Gv[i] is D-dim vector of the ith direction.
   */
  TinyVector<SingleParticlePos_t,D> Gv;
  //@}
  
  //angles between the two lattice vectors
  SingleParticlePos_t ABC;

  //@{
  /**@brief Parameters defining boundary Conditions */
  ///Functors that apply boundary conditions on the position vectors
  ParticleBConds<T,D> BConds;
  //@}

  ///default constructor, assign a huge supercell
  CrystalLattice();
  /** copy constructor
      @param rhs An existing SC object is copied to this SC.
  */
  CrystalLattice(const CrystalLattice<T,D>& rhs);
  
  ///destructor
  virtual ~CrystalLattice(){ }

  /**@param i the index of the directional vector, \f$i\in [0,D)\f$
   *@return The lattice vector of the ith direction
   *@brief Provide interfaces familiar to fotran users
   */
  inline SingleParticlePos_t a(int i) const {
    return Rv[i];
  }

  /**@param i the index of the directional vector, \f$i\in [0,D)\f$
   *@return The reciprocal vector of the ith direction
   *@brief Provide interfaces familiar to fotran users
   */
  inline SingleParticlePos_t b(int i) const {
    return Gv[i];
  }

  /** Convert a cartesian vector to a unit vector.
   * Boundary conditions are not applied.
   */
  template<class T1>
  inline SingleParticlePos_t toUnit(const TinyVector<T1,D> &r) const {
#ifdef OHMMS_LATTICEOPERATORS_H
    return DotProduct<TinyVector<T1,D>,Tensor<T,D>,ORTHO>::apply(r,G);    
#else
    return dot(r,G);
#endif
  }

  /** Convert a unit vector to a cartesian vector.
   * Boundary conditions are not applied.
   */
  template<class T1>
  inline SingleParticlePos_t toCart(const TinyVector<T1,D> &c) const {
#ifdef OHMMS_LATTICEOPERATORS_H
    return DotProduct<TinyVector<T1,D>,Tensor<T,D>,ORTHO>::apply(c,R);    
#else
    return dot(c,R);
#endif
  }


  /** evaluate the cartesian distance
   *@param ra a vector in the supercell unit
   *@param rb a vector in the supercell unit
   *@return Cartesian distance with two vectors in SC unit
   *
   @note The distance between two cartesian vectors are handled 
   *by dot function defined in OhmmsPETE/TinyVector.h
   */
  inline T Dot(const SingleParticlePos_t &ra, 
	       const SingleParticlePos_t &rb) const {
#ifdef OHMMS_LATTICEOPERATORS_H
    return CartesianNorm2<TinyVector<T,D>,Tensor<T,D>,ORTHO>::apply(ra,M,rb);
#else
    return dot(ra,dot(M,rb));
#endif
  }

  /** conversion of a reciprocal-vector 
   *@param kin an input reciprocal vector in the Reciprocal-vector unit
   *@return k(reciprocal vector) in cartesian unit
  */
  inline SingleParticlePos_t k_cart(const SingleParticlePos_t& kin) const {
#ifdef OHMMS_LATTICEOPERATORS_H
    return TWOPI*DotProduct<SingleParticlePos_t,Tensor_t,ORTHO>::apply(G,kin);
#else
    return TWOPI*dot(G,kin);
#endif
  }

  /** evaluate \f$k^2\f$
   *
   *@param kin an input reciprocal vector in reciprocal-vector unit
   *@return \f$k_{in}^2\f$ 
   */
  inline T ksq(const SingleParticlePos_t& kin) const {
#ifdef OHMMS_LATTICEOPERATORS_H
    return CartesianNorm2<TinyVector<T,D>,Tensor<T,D>,ORTHO>::apply(kin,Mg,kin);
#else
    return dot(kin,dot(Mg,kin));
#endif
  }

  ///assignment operator
  CrystalLattice<T,D,ORTHO>& operator=(const CrystalLattice<T,D,ORTHO>& rhs);

  /** assignment operator
   *@param rhs a tensor representing a unit cell
   */
  CrystalLattice<T,D,ORTHO>& operator=(const Tensor<T,D>& rhs);

  /** scale the lattice vectors by sc. All the internal data are reset.
   *@param sc the scaling value
   *@return a new CrystalLattice
   */
  CrystalLattice<T,D,ORTHO>& operator*=(T sc);
  
  /** set the lattice vector from the command-line options
   *@param argc the number of arguments
   *@param argv the argument lists
   *
   *This function is to provide a simple interface for testing.
   */
  void set(int argc, char **argv);
  
  /** set the lattice vector from the command-line options stored in a vector
   *@param argv the argument lists
   *
   *This function is to provide a simple interface for testing.
   */
  void set(std::vector<string>& argv);

  /** set the lattice vector by an array containing DxD T
   *@param sc a scalar to scale the input lattice parameters
   *@param lat the starting address of DxD T-elements representing a supercell
   */
  void set(T sc, T* lat= NULL);

  /** set the lattice vector by a CrystalLattice and expand it by integers
   *@param oldlat An input supercell to be copied.
   *@param uc An array to expand a supercell.
   */
  void set(const CrystalLattice<T,D,ORTHO>& oldlat, int* uc= NULL);

  /** set the lattice vector from the command-line options
   *@param lat a tensor representing a supercell
   */
  void set(const Tensor<T,D>& lat);

  /** Evaluate the reciprocal vectors, volume and metric tensor
   */
  void reset();

//  //@{
//  /* Copy functions with unit conversion*/
//  template<class PA> void convert(const PA& pin, PA& pout) const;
//  template<class PA> void convert2Unit(const PA& pin, PA& pout) const;
//  template<class PA> void convert2Cart(const PA& pin, PA& pout) const;
//  template<class PA> void convert2Unit(PA& pout) const;
//  template<class PA> void convert2Cart(PA& pout) const;
//  //@}
//
//  template<class PA> void applyBC(const PA& pin, PA& pout) const;
//  template<class PA> void applyBC(PA& pos) const;
//  template<class PA> void applyBC(const PA& pin, PA& pout, int first, int last) const;

  //! Print out CrystalLattice Data
  void print(ostream& , int level=2) const;
};

}
//including the definitions of the member functions
#include "Lattice/CrystalLattice.cpp"

#endif
  


/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/
