/* ----------------------------------------------------------------------
   SPPARKS - Stochastic Parallel PARticle Kinetic Simulator
   contact info, copyright info, etc
------------------------------------------------------------------------- */

#ifndef DIAG_CLUSTER2D_H
#define DIAG_CLUSTER2D_H

#include "stdio.h"
#include <stack>
#include "cluster.h"
#include "diag.h"

namespace SPPARKS {

class DiagCluster2d : public Diag {
  friend class SweepLattice2d;

 public:
  DiagCluster2d(class SPK *, int, char **);
  virtual ~DiagCluster2d();

  void init(double);
  void compute(double, int);
  void stats(char *);
  void stats_header(char *);

 protected:

  // Functions and Data for Cluster Analysis
  void analyze_clusters(double);
  void write_header();
  void dump_clusters(double);
  void dump_clusters_detailed(double);
  void generate_clusters();
  void add_cluster(int, double, int, double*);
  void free_clustlist();

  int** cluster_ids;
  int ncluster, ncluster_reduced;
  Cluster* clustlist;
  std::stack<int> cluststack;      // stack for performing cluster analysis

  FILE *fp, *fpdump;
  class AppLattice2d *applattice2d;

  int nx_global,ny_global,nx_procs,ny_procs,delghost;
  int nx_local,ny_local,nx_offset,ny_offset,nxlo,nylo,nxhi,nyhi;

  enum DumpStyles {STANDARD, OPENDX, DETAILED};
  int dump_style;
  int idump;
  char* opendxroot;
  int opendxcount;
};

}

#endif