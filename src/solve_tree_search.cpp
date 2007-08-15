/* ----------------------------------------------------------------------
   SPPARKS - Stochastic Parallel PARticle Kinetic Simulator
   contact info, copyright info, etc
------------------------------------------------------------------------- */

#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "solve_tree_search.h"
#include "spk.h"
#include "random_park.h"
#include "error.h"

using namespace SPPARKS;

/* ---------------------------------------------------------------------- */

SolveTreeSearch::SolveTreeSearch(SPK *spk, int narg, char **arg) : 
  Solve(spk, narg, arg)
{
  if (narg != 2) error->all("Illegal solve command");
  allocated = 0;
  int seed = atoi(arg[1]);
  random = new RandomPark(seed);
  tree = NULL;
}

/* ---------------------------------------------------------------------- */

SolveTreeSearch::~SolveTreeSearch()
{
  free_arrays();
  delete random;
}

/* ---------------------------------------------------------------------- */

void SolveTreeSearch::init(int n, double *propensity)
{
  int ntotal = 0;
  offset = 0;

  // memory allocation

  if(allocated) free_arrays();
  allocated = 1;

  nevents = n;
  sum = 0;

  // m = value such that 2^m >= nevents
  int m = 0;
  int neat = 1;

  while (neat < nevents) {neat *=2; m++;}

  // create tree of length ntotal
  // init all leaves to 0.0

  ntotal = 2*neat - 1;
  tree = new double[ntotal];
  offset = neat - 1;

  for (int i = 0; i < ntotal; i++) tree[i] = 0.0;

  for (int i = offset; i < offset + n; i++) tree[i] = propensity[i-offset];

  //  tree_to_screen(neat);
  sum_tree();
}

/* ---------------------------------------------------------------------- */

void SolveTreeSearch::update(int n, int *indices, double *propensity)
{
  for (int i = 0; i < n; i++) set(indices[i],propensity[indices[i]]);
}

/* ---------------------------------------------------------------------- */

void SolveTreeSearch::update(int n, double *propensity)
{
  set(n,propensity[n]);
}

/* ---------------------------------------------------------------------- */

void SolveTreeSearch::resize(int new_size, double *propensity)
{
  init(new_size, propensity);
}

/* ---------------------------------------------------------------------- */

int SolveTreeSearch::event(double *pdt)
{
  int m;
  double r2;
  double sumt = tree[0];

  if (sumt == 0.0) return -1;

  r2 = random->uniform();
  m = find(r2*sumt);
  
  *pdt = -1.0/sumt * log(random->uniform());
  return m;
}

/* ----------------------------------------------------------------------
   sum entire tree, all nodes are computed
------------------------------------------------------------------------- */

void SolveTreeSearch::sum_tree()
{
  int child1,child2;
  for (int parent = offset-1; parent >= 0; parent--) {
    child1 = 2*parent + 1;
    child2 = 2*parent + 2;
    tree[parent] = tree[child1] + tree[child2];
  }
}

/* ----------------------------------------------------------------------
   set propensity[i] to value
   recompute sum tree for all its ancestors
------------------------------------------------------------------------- */

void SolveTreeSearch::set(int i, double value)
{
  int parent,sibling;

  tree[offset+i] = value;

  // i walks tree from leaf to root, summing children at each step
  // left child is odd index, right child is even index

  i += offset;
  while (i > 0) {
    if (i % 2) sibling = i + 1;
    else sibling = i - 1;
    parent = (i-1)/2;
    tree[parent] = tree[i] + tree[sibling];
    i = parent;
  }
}

/* ----------------------------------------------------------------------
   value = uniform RN from 0 to tree[0]
   return index (0 to M-1) of propensity bin it falls in
------------------------------------------------------------------------- */

int SolveTreeSearch::find(double value)
{
  int i,leftchild;

  // i walks tree from root to appropriate leaf
  // value is modified when right branch of tree is traversed

  i = 0;
  while (i < offset) {
    leftchild = 2*i + 1;
    if (value <= tree[leftchild]) i = leftchild;
    else {
      value -= tree[leftchild];
      i = leftchild + 1;
    }
  }
  return i - offset;
}

/* ----------------------------------------------------------------------
   free arrays used by solver
------------------------------------------------------------------------- */

void SolveTreeSearch::free_arrays()
{
  delete [] tree;
}