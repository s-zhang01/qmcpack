#include <Configuration.h>
#include <spline/einspline_engine.hpp>
#include <OhmmsPETE/OhmmsArray.h>
#include <Message/Communicate.h>
#include <mpi/collectives.h>
#include <Utilities/RandomGenerator.h>
#include <Utilities/Timer.h>
#include <Utilities/IteratorUtility.h>
#include <Utilities/UtilityFunctions.h>
namespace qmcplusplus {
  template<typename EngT>
    class einspline_omp
    {
      typedef einspline_engine<EngT> spliner_type;
      enum {DIM=spliner_type::DIM};
      vector<spliner_type*> Engines;
      vector<int> Offsets;

      public:
      einspline_omp(){}

      ~einspline_omp()
      {
        delete_iter(Engines.begin(),Engines.end());
      }

      void create_plan(const TinyVector<int,DIM>& ng, int nsplines, bool randomize=false)
      {
        if(Engines.empty())
          Engines.resize(omp_get_max_threads(),0);

        Offsets.resize(Engines.size()+1);
        FairDivideLow(nsplines,Engines.size(),Offsets);
        
#pragma omp parallel
        {
          int ip=omp_get_thread_num();
          int ns_loc=Offsets[ip+1]-Offsets[ip];//nsplines/Engines.size();
          if(Engines[ip]) delete Engines[ip];
          Engines[ip]=new spliner_type(ng,ns_loc,Offsets[ip]);
          if(randomize)
          {
            Array<typename spliner_type::value_type, 3> data(ng[0], ng[1], ng[2]);
            for (int i = 0; i < ns_loc; ++i)
            {
              for (int j = 0; j < data.size(); ++j)
                data(j) = Random();
              Engines[ip]->set(i, data.data(), data.size());
            }
          }
        }
      }

      template<typename T1>
      void evaluate_v(const TinyVector<T1,DIM>& here)
      {
        const int np=Engines.size();
#pragma omp parallel for
        for(int ip=0; ip<np; ++ip) Engines[ip]->evaluate_v(here);
      }

      template<typename T1>
      void evaluate_v(const vector<TinyVector<T1,DIM> >& here)
      {
        const int np=Engines.size();
#pragma omp parallel for
        for(int ip=0; ip<np; ++ip) 
        {
          for(int i=0; i<here.size(); ++i)
            Engines[ip]->evaluate_v(here[i]);
        }
      }

      template<typename T1>
      void evaluate_vgh(const TinyVector<T1,DIM>& here)
      {
        const int np=Engines.size();
#pragma omp parallel for
        for(int ip=0; ip<np; ++ip) Engines[ip]->evaluate_vgh(here);
      }
    };
}

using namespace qmcplusplus;

int main(int argc, char **argv)
{

  OHMMS::Controller->initialize(argc,argv);
  Communicate* mycomm=OHMMS::Controller;
  OhmmsInfo Welcome("mixed",mycomm->rank());
  Random.init(0,1,11);
  //einspline_test<multi_UBspline_3d_s> sp_s;
  //einspline_test<multi_UBspline_3d_d> sp_d;
  //einspline_test<multi_UBspline_3d_c> sp_c;
  //einspline_test<multi_UBspline_3d_z> sp_z;

  typedef double value_type;
  typedef double real_type;

  TinyVector<int,3> ng(48);
  int num_splines=128;
  int num_samples=14;
  int niters=1000;
  int opt;

  while((opt = getopt(argc, argv, "hg:x:y:z:i:s:p:")) != -1) {
    switch(opt) {
      case 'h':
        printf("[-g grid| -x grid_x -y grid_y -z grid_z] -s states -p particles -i iterations\n");
        return 1;
      case 'g':
        ng=atoi(optarg);
        break;
      case 'x':
        ng[0]=atoi(optarg);
        break;
      case 'y':
        ng[1]=atoi(optarg);
        break;
      case 'z':
        ng[2]=atoi(optarg);
        break;
      case 's':
        num_splines=atoi(optarg);
        break;
      case 'p':
        num_samples=atoi(optarg);
        break;
      case 'i':
        niters=atoi(optarg);
        break;
    }
  }

  einspline_omp<multi_UBspline_3d_d> myeng;
  myeng.create_plan(ng,num_splines,true);
  RandomGenerator_t rng;
  Timer clock;
  for (int i = 0; i < niters; ++i)
  {
    vector<TinyVector<real_type, 3> > here(num_samples);
    for(int k=0; k<num_samples; ++k)
      here[k]=TinyVector<real_type,3>(rng(), rng(), rng());
    myeng.evaluate_v(here);
  }
  double dt_v=clock.elapsed();
  for (int i = 0; i < niters; ++i)
  {
    TinyVector<real_type, 3> here(rng(), rng(), rng());
    myeng.evaluate_vgh(here);
  }
  double dt_vgh=clock.elapsed();

  mycomm->barrier();

  mpi::allreduce(*mycomm,dt_v);
  mpi::allreduce(*mycomm,dt_vgh);

  dt_v/=static_cast<double>(mycomm->size());
  dt_vgh/=static_cast<double>(mycomm->size());

  num_splines *= mycomm->size();
  app_log().setf(std::ios::scientific, std::ios::floatfield);
  app_log().precision(6);
  app_log() << "einspline_omp " << setw(3) << omp_get_max_threads() 
    <<setw(6) << ng[0] << setw(6) << num_splines << setw(6) << num_samples
    << "  " << niters*num_samples*num_splines/dt_v
    << "  " << niters*num_splines/dt_vgh
    << endl;

  return 0;
}