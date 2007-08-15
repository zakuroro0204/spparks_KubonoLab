/* ----------------------------------------------------------------------
   SPPARKS - Stochastic Parallel PARticle Kinetic Simulator
   contact info, copyright info, etc
------------------------------------------------------------------------- */

#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "solve_group_search.h"
#include "groups.h"
#include "spk.h"
#include "random_park.h"
#include "error.h"

using namespace SPPARKS;

/* ---------------------------------------------------------------------- */

SolveGroupSearch::SolveGroupSearch(SPK *spk, int narg, char **arg) : 
  Solve(spk, narg, arg)
{
  if (narg < 4) error->all("Illegal solve command");
  
  lo = atof(arg[1]);
  hi = atof(arg[2]);

  ngroups_in = 0; ngroups_flag = false;
  if (narg == 5) {
    ngroups_in = atoi(arg[3]); 
    ngroups_flag = true; 
    seed = atoi(arg[4]);
  }
  else seed = atoi(arg[3]);

  random = new RandomPark(seed);
  groups = new Groups(lo, hi, seed, ngroups_flag, ngroups_in);
  p = NULL;
}

/* ---------------------------------------------------------------------- */

SolveGroupSearch::~SolveGroupSearch()
{
  delete random;
  delete groups;
  delete [] p;
}

/* ---------------------------------------------------------------------- */

void SolveGroupSearch::init(int n, double *propensity)
{
  nevents = n;
  sum = 0;
  delete [] p;
  p = new double[n+10];

  last_size = n;

  for (int i = 0; i < n; i++) {
    double pt = propensity[i];
    if (lo > pt) {
      lo = pt; 
      fprintf(screen, "Lower bound violated. Reset to %g \n", lo);
    }
    else if (hi < pt)  {
      hi = pt; 
      fprintf(screen, "Upper bound violated. Reset to %g \n", hi);
    }
    p[i] = pt;
    sum += pt;
  }

  groups->partition_init(p, n, n+10);
}

/* ---------------------------------------------------------------------- */

void SolveGroupSearch::update(int n, int *indices, double *propensity)
{
  for (int i = 0; i < n; i++) {
    int j = indices[i];
    double pt = p[j];
    if(propensity[j]!=pt){
      sum -= pt;
      groups->alter_element(j, p, pt);
      p[j] = propensity[j];
      sum +=  p[j];
    }
  }
}
/* ---------------------------------------------------------------------- */

void SolveGroupSearch::update(int n, double *propensity)
{
  double pt = p[n];

  if(propensity[n]!=pt){
    sum -= pt;
    groups->alter_element(n, p, pt);
    p[n] = propensity[n];
    sum += p[n];
  }
}

/* ---------------------------------------------------------------------- */

void SolveGroupSearch::resize(int new_size, double *propensity)
{
  init(new_size, propensity);
}

/* ---------------------------------------------------------------------- */
int SolveGroupSearch::event(double *pdt)
{
  int m;

  if (sum == 0.0) return -1;
  m = groups->sample(p);

  *pdt = -1.0/sum * log(random->uniform());
  return m;
}
