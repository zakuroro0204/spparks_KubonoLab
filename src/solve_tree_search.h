/* ----------------------------------------------------------------------
   SPPARKS - Stochastic Parallel PARticle Kinetic Simulator
   contact info, copyright info, etc
------------------------------------------------------------------------- */

#ifndef SOLVE_TREE_SEARCH_H
#define SOLVE_TREE_SEARCH_H

#include "solve.h"

namespace SPPARKS {

class SolveTreeSearch : public Solve {
 public:
  SolveTreeSearch(class SPK *, int, char **);
  ~SolveTreeSearch();
  void input(int, char **) {}
  void init(int, double *);
  void update(int, int *, double *);
  void update(int, double *);
  void resize(int, double *);
  int event(double *);
  void sum_tree();
  void free_arrays();
  void set(int , double);
  int find(double);
  void tree_to_screen(int);

 private:
  class RandomPark *random;
  int nevents;
  double sum;

  double *tree;
  int offset;

  int allocated;
};

}

#endif