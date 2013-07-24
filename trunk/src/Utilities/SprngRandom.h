//////////////////////////////////////////////////////////////////
// (c) Copyright 1998-2002 by Jeongnim Kim
//
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
/** @file SprngRandom.h
 * @brief Declaration of SprngRandom
 */
#ifndef OHMMS_SPRNGRANDOM_H
#define OHMMS_SPRNGRANDOM_H
#include <math.h>
#include <sprng.h>

#define SRSEED 985456376

/**   A wrapper class to generate a random number using sprng library.
 *
 *  The template parameter rg is to set the generator type.
 *  Choose 0 - 5: EXAMPLE/gen_types_menu.h
 *  \li{    printf("   lfg     --- 0 \n");}
 *  \li{    printf("   lcg     --- 1 \n");}
 *  \li{    printf("   lcg64   --- 2 \n");}
 *  \li{    printf("   cmrg    --- 3 \n");}
 *  \li{    printf("   mlfg    --- 4 \n");}
 *  \li{    printf("   pmlcg   --- 5 \n");}
 *
 *  Documentation at http://sprng.cs.fsu.edu
 */
template<int RNGT>
class SprngRandom
{

public:
  typedef double Return_t;

  /** default constructor */
  SprngRandom(): myContext(0), nContexts(1), myStream(0),baseSeed(SRSEED)  { }

  /** copyc constructor */
  SprngRandom(const SprngRandom& rng): myStream(0),baseSeed(SRSEED)
  {
    init(rng.myContext,rng.nContexts,(*rng.generator)());
  }

  /** constructor
   *@param i thread ID
   *@param nstr number of streams
   *@param iseed random number seed
   */
  SprngRandom(int i, int nstr, int iseed):
    myStream(0),baseSeed(SRSEED)
  {
    init(i,nstr,iseed);
  }

  ~SprngRandom()
  {
    if(myStream)
      free_stream(myStream);
  }

  /** initialize the stream
   *
   * @if iseed < 0
   *  random number seed is generated by sprng library
   * @else
   *  use iseed as the random number sed
   * @endif
   */
  inline void init(int i, int nstr, int iseed)
  {
    if(myStream)
      free_stream(myStream);
    myContext = i;
    nContexts = nstr;
    if(iseed < 0)
      // generate a new seed
    {
      baseSeed = make_sprng_seed();
    }
    else
      if(iseed > 0)
        // use input seed
      {
        baseSeed = iseed;
      } // if iseed = 0, use SRSEED
#if SPRNG_VERSION == 1
    myStream = init_sprng(RNGT,myContext,nContexts,baseSeed);
#elif SPRNG_VERSION == 2
    myStream = init_sprng(RNGT,myContext,nContexts,baseSeed, SPRNG_DEFAULT);
#else
#error "Unsupported sprng library. Only versions 1 and 2 are supported."
#endif
  }

  inline Return_t getRandom()
  {
    return sprng(myStream); //!< return [0,1)
  }
  inline Return_t operator()()
  {
    return getRandom(); //!< return [0,1)
  }
  inline int irand()
  {
    return isprng(myStream); //!< return random integer
  }
  inline void reset()
  {
    if(myStream)
      free_stream(myStream);
    baseSeed = make_sprng_seed();
#if SPRNG_VERSION == 1
    myStream = init_sprng(RNGT,myContext,nContexts,baseSeed);
#elif SPRNG_VERSION == 2
    myStream = init_sprng(RNGT,myContext,nContexts,baseSeed, SPRNG_DEFAULT);
#else
#error "Unsupported sprng library. Only versions 1 and 2 are supported."
#endif
  }

  inline void read(std::istream& rin)
  {
  }

  inline void write(std::ostream& rout) const
  {
  }

// inline void bivariate(Return_t& g1, Return_t &g2) {
//   Return_t v1, v2, r;
//   do {
//   v1 = 2.0e0*getRandom() - 1.0e0;
//   v2 = 2.0e0*getRandom() - 1.0e0;
//   r = v1*v1+v2*v2;
//   } while(r > 1.0e0);
//   Return_t fac = sqrt(-2.0e0*log(r)/r);
//   g1 = v1*fac;
//   g2 = v2*fac;
// }

private:
  int myContext;
  int nContexts;
  int baseSeed;
  int* myStream;
};
#endif

/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$
 ***************************************************************************/
