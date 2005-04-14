//////////////////////////////////////////////////////////////////
// (c) Copyright 2003  by Jeongnim Kim and Jordan Vincent
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
//////////////////////////////////////////////////////////////////
// -*- C++ -*-
#ifndef OHMMS_GRID_FUNCTOR_H
#define OHMMS_GRID_FUNCTOR_H

#include "Numerics/OneDimGridBase.h"

template<class T, unsigned D>
struct FunctorBase { };

/** Implement One-Dimensional function on a radial grid. 
 *\brief Store the values of the function for the 
 *cooresponding grid points, \f$ y_i = y(x_i) \f$.  
 */

template <class Td, 
	  class Tg = Td, 
	  class CTd= Vector<Td>,
	  class CTg= Vector<Tg>  >
struct OneDimGridFunctor//: public FunctorBase<Td,1> {
{
  typedef Td  value_type;
  typedef Tg  point_type;
  typedef CTd data_type;
  typedef OneDimGridBase<Tg,CTg> grid_type;
  typedef OneDimGridFunctor<Td,Tg,CTd,CTg>  this_type;

  /**constructor
   *@param gt the radial grid
   */
  OneDimGridFunctor(grid_type* gt = 0): m_grid(gt) {
    if(m_grid) resize(m_grid->size());
    FirstAddress.resize(3,0);
  }

  inline virtual ~OneDimGridFunctor() { }

  ///copy constructor
  OneDimGridFunctor(const this_type& a): m_grid(a.m_grid){
    if(m_grid) resize(m_grid->size());
    FirstAddress.resize(3,0);
  }

  ///assignment operator
  const this_type& operator=(const this_type& a) {
    m_grid = a.m_grid;
    m_Y = a.m_Y;
    //m_Y2 = a.m_Y2;
    return *this;
  }

  template<class T1>
  const this_type& operator=(const T1& x) {
    Y = x;
    return *this;
  }

  ///reset the values: do nothing
  virtual void reset() { }

  ///set the number of nodes
  inline void setNumOfNodes(int n) { NumNodes = n;}

  ///return the number of nodes
  inline int getNumOfNodes() const { return NumNodes;}

  ///return the grid data
  inline value_type* data() { return &(m_Y[0]);}
  ///assign the grid data
  inline const value_type* data() const { return &(m_Y[0]);}
  ///return the number of data points
  inline int size() const { return m_Y.size();}
  ///resize the number of data points
  inline void resize(int n) { m_Y.resize(n);}
  ///return the radial grid
  inline const grid_type& grid() const { return *m_grid;}
  ///assign a radial grid
  inline grid_type& grid() { return *m_grid;}
  virtual int setgrid(point_type r)=0;
  /////set the index of the grid for radius r
  //inline int setgrid(point_type r) {
  //  return m_grid->index(r);
  //}

  /** return the address of the values
   * @param i index, i=0 value, i=1 first derivative, i=2 second
   */
  inline value_type* data(int i) {
    return FirstAddress[i];
  }
  /**return the differntial spacing for the grid
   *@warning only for LinearGrid and LogGrid
  */
  inline point_type dh() const { return m_grid->dh();}

  ///return \f$r(i)\f$ the grid point at index i
  inline point_type r(int i) const { return m_grid->r(i);}
  ///return \f$r(i+1)-r(i)\f$
  inline point_type dr(int i) const { return m_grid->dr(i);}
 
  /**
   *@param r radial distance
   *@return the value of the function
   *@brief Evaluate the function and its derivatives, store the
   *values of the derivatives.  
   *@note This should not be called frequently: only for the 
   *transform function (Transform2GridFunctor)
   */
  inline value_type f(point_type r) {
    setgrid(r);
    return Y=splint(r,dY,d2Y);
  }

  /**
   *@param r radial distance
   *@return the derivative of the function
   *@brief Evaluate the function and its derivatives, store the
   *values of the derivatives.  
   *@note This should not be called frequently: only for the 
   *transform function (Transform2GridFunctor)
   */
  inline value_type df(point_type r) {
    setgrid(r);
    splint(r,dY,d2Y);
    return dY;
  }

  ///returns a value
  inline value_type operator()(int i) const { return m_Y[i];}
  ///assign a value
  inline value_type& operator()(int i) { return m_Y[i];} 

  /** Evaluate the function value only
   */
  inline value_type evaluate(point_type r, point_type rinv) {
    return Y = splint(r);
  }

  /**Evaluate the function and its derivatives.
   *@note Must first call the function setgrid to
   *determine the interval on the grid which contains r.
   */
  inline value_type evaluateAll(point_type r, point_type rinv) {
    return Y = splint(r,dY,d2Y);
  }

  virtual 
  value_type 
  splint(point_type r, value_type& du, value_type& d2u) { return 0.0; }

  virtual 
  value_type splint(point_type r) { return 0.0; }
   
  virtual 
  void spline(int imin, value_type yp1, int imax, value_type ypn) {  }

  virtual void spline() {  }

  /**
   *@param r radial distance
   *@param rinv inverse of radial distance
   *@param du return derivative
   *@param d2u return 2nd derivative
   *@return the value of the function 
   *@brief Evaluate the function and its derivatives.
   */
  inline value_type 
  evaluate(point_type r, point_type rinv, value_type& du, value_type& d2u) {
    return splint(r,du,d2u);
  }

  ///pointer to the radial grid
  grid_type* m_grid;

  ///store the value of the function
  value_type Y;
  ///store the derivative of the function
  value_type dY;
  ///store the second derivative of the function
  value_type d2Y;

  ///data for the function on the grid
  data_type m_Y;  

  ///the number of nodes
  int NumNodes;

  ///address
  vector<value_type*> FirstAddress;
};
#endif
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/
