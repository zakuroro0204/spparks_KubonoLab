/* ----------------------------------------------------------------------
   SPPARKS - Stochastic Parallel PARticle Kinetic Simulator
   http://www.cs.sandia.gov/~sjplimp/spparks.html
   Steve Plimpton, sjplimp@sandia.gov, Sandia National Laboratories

   Copyright (2008) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the GNU General Public License.

   See the README file in the top-level SPPARKS directory.
------------------------------------------------------------------------- */

#include "comm_off_lattice.h"
#include "app_off_lattice.h"
#include "memory.h"
#include "error.h"

using namespace SPPARKS_NS;

#include <map>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define DELTA 16384

#define BUFFACTOR 1.5

enum{INTERIOR,EDGE,GHOST};        // same as in app_off_lattice.cpp

/* ---------------------------------------------------------------------- */

CommOffLattice::CommOffLattice(SPPARKS *spk) : Pointers(spk)
{
  MPI_Comm_rank(world,&me);
  MPI_Comm_size(world,&nprocs);

  allswap = NULL;
  sectorswap = NULL;
  sectorreverseswap = NULL;
  nsector = 0;

  size_one = 5;
  smax = rmax = 0;
  sbuf = rbuf = NULL;
}

/* ---------------------------------------------------------------------- */

CommOffLattice::~CommOffLattice()
{
  if (allswap) free_swap(allswap);
  if (sectorswap) {
    for (int i = 0; i < nsector; i++) free_swap(sectorswap[i]);
    delete [] sectorswap;
  }
  if (sectorreverseswap) {
    for (int i = 0; i < nsector; i++) free_swap(sectorreverseswap[i]);
    delete [] sectorreverseswap;
  }

  memory->sfree(sbuf);
  memory->sfree(rbuf);
}

/* ----------------------------------------------------------------------
   setup comm pattern
   3 kinds of patterns: all ghosts, sector ghosts, sector reverse
   if sectorflag = 0, just ghosts for entire proc domain 
   if sectorflag = 1, do all and sector ghosts, reverse if needed
   array = NULL = communicate iarray/darray from app
   array = non-NULL = communicate passed-in array (from diagnostic)
------------------------------------------------------------------------- */

void CommOffLattice::init(int nsector_request)
{
  appoff = (AppOffLattice *) app;

  ninteger = appoff->ninteger;
  ndouble = appoff->ndouble;

  xprd = appoff->xprd;
  yprd = appoff->yprd;
  zprd = appoff->zprd;

  // clear out old swaps

  if (allswap) free_swap(allswap);
  if (sectorswap) {
    for (int i = 0; i < nsector; i++) free_swap(sectorswap[i]);
    delete [] sectorswap;
  }
  if (sectorreverseswap) {
    for (int i = 0; i < nsector; i++) free_swap(sectorreverseswap[i]);
    delete [] sectorreverseswap;
  }
  allswap = NULL;
  sectorswap = NULL;
  sectorreverseswap = NULL;

  // create new swaps as requested

  allswap = create_swap_all();

  nsector = nsector_request;
  if (nsector > 1) {
    setup_sector_chunks(appoff->nbinx,appoff->nbiny,appoff->nbinz);
    sectorswap = new Swap*[nsector];
    //sectorreverseswap = new Swap*[nsector];
    for (int i = 0; i < nsector; i++) {
      sectorswap[i] = create_swap_sector(i);
      //sectorreverseswap[i] = create_swap_sector_reverse(i);
    }
  }
}

/* ----------------------------------------------------------------------
   acquire ghost values for entire proc sub-domain
------------------------------------------------------------------------- */

void CommOffLattice::all()
{
  perform_swap(allswap);
}

/* ----------------------------------------------------------------------
   acquire ghost values for one sector
------------------------------------------------------------------------- */

void CommOffLattice::sector(int isector)
{
  perform_swap(sectorswap[isector]);
}

/* ----------------------------------------------------------------------
   reverse communicate changed border values for one sector
------------------------------------------------------------------------- */

void CommOffLattice::reverse_sector(int isector)
{
  //perform_swap(sectorreverseswap[isector]);
}

/* ----------------------------------------------------------------------
   create a Swap communication pattern
   acquire ghost bins for my entire subdomain
------------------------------------------------------------------------- */

CommOffLattice::Swap *CommOffLattice::create_swap_all()
{
  int i,m,n;

  int nbins = appoff->nbins;
  int *binflag = appoff->binflag;
  int *nimages = appoff->nimages;
  int **imageindex = appoff->imageindex;
  int **imageproc = appoff->imageproc;

  // add all image bins to list

  for (m = 0; m < nbins; m++)
    if (binflag[m] == EDGE) n += nimages[m];

  // allocate list

  int **list = memory->create_2d_int_array(n,3,"commoff:list");

  // fill list with info on all image bins

  n = 0;
  for (m = 0; m < nbins; m++)
    if (binflag[m] == EDGE)
      for (i = 0; i < nimages[m]; i++) {
	list[n][0] = m;
	list[n][1] = imageindex[m][i];
	list[n][2] = imageproc[m][i];
	n++;
      } 

  // create swap based on list of bins to send

  Swap *swap = new Swap;

  create_send_from_list(n,list,swap);
  create_recv_from_send(swap);

  memory->destroy_2d_int_array(list);

  return swap;
}

/* ----------------------------------------------------------------------
   create a Swap communication pattern
   acquire ghost bins for a single sector of my subdomain
------------------------------------------------------------------------- */

CommOffLattice::Swap *CommOffLattice::create_swap_sector(int isector)
{
  int i,m,n;

  int nbins = appoff->nbins;
  int nbinx = appoff->nbinx;
  int nbiny = appoff->nbiny;
  int nbinz = appoff->nbinz;
  int *nimages = appoff->nimages;
  int *binflag = appoff->binflag;
  int **imageindex = appoff->imageindex;
  int **imageproc = appoff->imageproc;

  // add only image bins to list needed for isector

  for (m = 0; m < nbins; m++)
    if (binflag[m] == EDGE)
      for (i = 0; i < nimages[m]; i++)
	if (bin_sector_ghost(isector,imageindex[m][i],nbinx,nbiny,nbinz)) n++;

  // allocate list

  int **list = memory->create_2d_int_array(n,3,"commoff:list");

  // fill list with info on all image bins for isector

  n = 0;
  for (m = 0; m < nbins; m++)
    if (binflag[m] == EDGE)
      for (i = 0; i < nimages[m]; i++)
	if (bin_sector_ghost(isector,imageindex[m][i],nbinx,nbiny,nbinz)) {
	  list[n][0] = m;
	  list[n][1] = imageindex[m][i];
	  list[n][2] = imageproc[m][i];
	  n++;
	}

  // create swap based on list of bins to send

  Swap *swap = new Swap;

  create_send_from_list(n,list,swap);
  create_recv_from_send(swap);

  memory->destroy_2d_int_array(list);

  return swap;
}

/* ----------------------------------------------------------------------
   create a Swap communication pattern
   reverse comm of a single sector
------------------------------------------------------------------------- */

CommOffLattice::Swap *CommOffLattice::create_swap_sector_reverse(int isector)
{
  Swap *swap = new Swap;
  return swap;
}

/* ----------------------------------------------------------------------
   create send portion of a Swap communication pattern
   create from list of bins I send
------------------------------------------------------------------------- */

void CommOffLattice::create_send_from_list(int n, int **list, Swap *swap)
{
  int i,isend;

  int nbins = appoff->nbins;
  int *binflag = appoff->binflag;
  int *nimages = appoff->nimages;
  int **imageindex = appoff->imageindex;

  int *proc = new int[nprocs];
  for (i = 0; i < nprocs; i++) proc[i] = 0;
  for (i = 0; i < n; i++) proc[list[i][2]]++;

  int nsend = 0;
  for (i = 0; i < nprocs; i++)
    if (proc[i]) nsend++;
  if (proc[me]) nsend--;

  // should this loop stagger the proc order

  int *sproc = new int[nsend];
  int *scount = new int[nsend];
  for (isend = 0; isend < nsend; isend++) scount[isend] = 0;
  int *stotal = new int[nsend];

  nsend = 0;
  for (i = 0; i < nprocs; i++)
    if (proc[i] && i != me) {
      proc[i] = nsend;
      sproc[nsend++] = i;
    }

  int ccount = proc[me];
  int *cbinsrc = NULL;
  int *cbindest = NULL;
  if (ccount) {
    cbinsrc = new int[ccount];
    cbindest = new int[ccount];
  }

  ccount = 0;
  for (i = 0; i < n; i++) {
    if (list[i][2] == me) {
      cbinsrc[ccount] = list[i][0];
      cbindest[ccount] = list[i][1];
      ccount++;
    } else {
      isend = proc[list[i][2]];
      scount[isend]++;
    }
  }

  int **sindex = 
    memory->create_2d_int_ragged_array(nsend,scount,"commoff:sindex");
  int **ssite = 
    memory->create_2d_int_ragged_array(nsend,scount,"commoff:ssite");

  for (isend = 0; isend < nsend; isend++) scount[isend] = 0;

  for (i = 0; i < n; i++) {
    if (list[i][2] != me) {
      isend = proc[list[i][2]];
      sindex[isend][scount[isend]] = list[i][0];
      ssite[isend][scount[isend]] = list[i][1];
      scount[isend]++;
    }
  }

  delete [] proc;

  // fill in swap data structure with send and copy info

  swap->nsend = nsend;
  swap->sproc = sproc;
  swap->scount = scount;
  swap->sindex = sindex;
  swap->ssite = ssite;
  swap->stotal = stotal;

  if (ccount) {
    swap->ncopy = 1;
    swap->ccount = ccount;
    swap->cbinsrc = cbinsrc;
    swap->cbindest = cbindest;
  } else swap->ncopy = 0;
}

/* ----------------------------------------------------------------------
   create recv portion of a Swap communication pattern
   start with list of sites I will send
   circulate list to all procs
------------------------------------------------------------------------- */

void CommOffLattice::create_recv_from_send(Swap *swap)
{
  int i;

  int nrecv = 0;

  int *rproc = new int[nrecv];
  int *rcount = new int[nrecv];
  int *rtotal = new int[nrecv];

  int **rindex = 
    memory->create_2d_int_ragged_array(nrecv,rcount,"commoff:rindex");
  int **rsite = 
    memory->create_2d_int_ragged_array(nrecv,rcount,"commoff:rsite");

  // fill in swap data structure with recv info

  swap->nrecv = nrecv;
  swap->rproc = rproc;
  swap->rcount = rcount;
  swap->rindex = rindex;
  swap->rsite = rsite;
  swap->rtotal = rtotal;
}

/* ----------------------------------------------------------------------
   free a Swap object
------------------------------------------------------------------------- */

void CommOffLattice::free_swap(Swap *swap)
{
  delete [] swap->sproc;
  delete [] swap->scount;
  memory->destroy_2d_int_array(swap->sindex);
  memory->destroy_2d_int_array(swap->ssite);
  delete [] swap->stotal;

  delete [] swap->rproc;
  delete [] swap->rcount;
  memory->destroy_2d_int_array(swap->rindex);
  memory->destroy_2d_int_array(swap->rsite);
  delete [] swap->rtotal;

  delete [] swap->cbinsrc;
  delete [] swap->cbindest;

  delete [] swap->request;
  delete [] swap->status;

  delete swap;
}

/* ----------------------------------------------------------------------
   communicate site values via Swap instructions
   setup per-site arrays for new ghost sites: bin,next,prev,nextimage
------------------------------------------------------------------------- */

void CommOffLattice::perform_swap(Swap *swap)
{
  int i,j,m,n,ibin,jbin,oldmax,count,total;

  // app info

  int *binhead = appoff->binhead;
  int **pbcoffset = appoff->pbcoffset;
  int *next = appoff->next;
  int *nextimage = appoff->nextimage;
  int nlocal = appoff->nlocal;

  // if no sectors on single proc, unset nextimage() for all owned sites

  if (nsector == 1)
    for (int i = 0; i < nlocal; i++) nextimage[i] = -1;

  // delete all ghosts, since are replacing them

  appoff->delete_all_ghosts();

  // post receives for bin counts

  for (int irecv = 0; irecv < swap->nrecv; irecv++)
    MPI_Irecv(swap->rsite[irecv],swap->rcount[irecv],MPI_INT,
	      swap->rproc[irecv],0,world,&swap->request[irecv]);

  // barrier to insure receives are posted

  MPI_Barrier(world);

  // count sites in each bin I send
  // sendmax = max # of sites to send to one proc

  int sendmax = 0;

  for (int isend = 0; isend < swap->nsend; isend++) {
    total = 0;
    for (n = 0; n < swap->scount[isend]; n++) {
      count = 0;
      m = binhead[swap->sindex[isend][n]];
      while (m >= 0) {
	count++;
	m = next[m];
      }
      swap->ssite[isend][n] = count;
      total += count;
    }
    swap->stotal[isend] = total;
    sendmax = MAX(sendmax,total);
  }

  // send bin counts

  for (int isend = 0; isend < swap->nsend; isend++)
    MPI_Send(swap->ssite[isend],swap->scount[isend],MPI_INT,
	     swap->sproc[isend],0,world);

  // copycount = # of ghosts generated via copying from my own bins

  int copycount = 0;

  if (swap->ncopy)
    for (n = 0; n < swap->ccount; n++) {
      m = binhead[swap->cbinsrc[n]];
      while (m >= 0) {
	copycount++;
	m = next[m];
      }
    }

  // wait on all incoming bin counts

  if (swap->nrecv) MPI_Waitall(swap->nrecv,swap->request,swap->status);

  // recvcount = # of ghosts received from other procs

  int recvcount = 0;

  for (int irecv = 0; irecv < swap->nrecv; irecv++) {
    total = 0;
    for (n = 0; n < swap->rcount[irecv]; n++)
      total += swap->rsite[irecv][n];
    swap->rtotal[irecv] = total;
    recvcount += total;
  }

  // trigger app to grow per-site arrays if new ghosts exceed limit

  int nmax = appoff->nlocal + recvcount + copycount;
  while (nmax > appoff->nmax) {
    oldmax = appoff->nmax;
    appoff->grow(0);
    appoff->add_free(oldmax);
  }

  // reset site ptrs from app

  int *id = appoff->id;
  double **xyz = appoff->xyz;
  int *bin = appoff->bin;

  int *site = appoff->site;

  next = appoff->next;
  nextimage = appoff->nextimage;

  // grow send/receive buffers as needed

  if (sendmax > smax) {
    smax = static_cast<int> (BUFFACTOR*sendmax);
    memory->sfree(sbuf);
    sbuf = (double *) memory->smalloc(smax*size_one*sizeof(double),"app:sbuf");
  }

  if (recvcount > rmax) {
    rmax = static_cast<int> (BUFFACTOR*recvcount);
    memory->sfree(rbuf);
    rbuf = (double *) memory->smalloc(rmax*size_one*sizeof(double),"app:rbuf");
  }

  // post receives for sites

  int offset = 0;
  for (int irecv = 0; irecv < swap->nrecv; irecv++) {
    MPI_Irecv(&rbuf[offset],swap->rtotal[irecv],MPI_INT,
	      swap->rproc[irecv],0,world,&swap->request[irecv]);
    offset += swap->rtotal[irecv];
  }

  // barrier to insure receives are posted

  MPI_Barrier(world);

  // send sites

  for (int isend = 0; isend < swap->nsend; isend++) {
    i = 0;
    for (n = 0; n < swap->scount[isend]; n++) {
      m = binhead[swap->sindex[isend][n]];
      while (m >= 0) {
	sbuf[i++] = id[m];
	sbuf[i++] = xyz[m][0];
	sbuf[i++] = xyz[m][1];
	sbuf[i++] = xyz[m][2];
	sbuf[i++] = site[m];
	m = next[m];
      }
    }

    MPI_Send(sbuf,swap->stotal[isend],MPI_INT,swap->sproc[isend],0,world);
  }

  // perform on-processor copies across PBCs
  // add ghost sites one at a time
  // if needed, setup nextimage chain of ptrs for these sites

  if (swap->ncopy)
    for (m = 0; m < swap->ccount; m++) {
      ibin = swap->cbinsrc[m];
      jbin = swap->cbindest[m];
      i = binhead[ibin];
      while (i >= 0) {
	j = appoff->new_ghost();
	id[j] = id[i];
	xyz[j][0] = xyz[i][0] + pbcoffset[jbin][0]*xprd;
	xyz[j][1] = xyz[i][1] + pbcoffset[jbin][1]*yprd;
	xyz[j][2] = xyz[i][2] + pbcoffset[jbin][2]*zprd;
	site[j] = site[i];
	bin[j] = jbin;
	appoff->add_to_bin(j,jbin);
	if (nsector == 1) {
	  nextimage[j] = nextimage[i];
	  nextimage[i] = j;
	}
	i = next[i];
      }
    }

  // wait on all incoming sites

  if (swap->nrecv) MPI_Waitall(swap->nrecv,swap->request,swap->status);

  // unpack received sites from each proc
  // add sites to my bins, adding in PBC if necessary

  m = 0;
  for (int irecv = 0; irecv < swap->nrecv; irecv++) {
    for (n = 0; n < swap->rcount[irecv]; n++) {
      for (i = 0; i < swap->rsite[irecv][n]; i++) {
	j = appoff->new_ghost();
	jbin = swap->rindex[irecv][n];
	id[j] = static_cast<int> (rbuf[m++]);
	xyz[j][0] = rbuf[m++] + pbcoffset[jbin][0]*xprd;
	xyz[j][1] = rbuf[m++] + pbcoffset[jbin][1]*yprd;
	xyz[j][2] = rbuf[m++] + pbcoffset[jbin][2]*zprd;
	site[j] = static_cast<int> (rbuf[m++]);
	bin[j] = jbin;
	appoff->add_to_bin(j,jbin);
      }
    }
  }
}

/*
reverse_comm:
  pre-store list of ghost bins in a sector
  pre-store owning proc of each ghost bin
  loop over those bins:
    if an owned particle is in that bin, ship it to unique owning proc
    have to add in PBC effect
    error check that recv proc puts it correct bin
*/

/* ----------------------------------------------------------------------
   return 1 if ibin is a ghost of sector isector
   ibin is a local ghost bin ID
   nbinx,nbiny,nbinz = extent of local bins
------------------------------------------------------------------------- */

int CommOffLattice::bin_sector_ghost(int isector, int ibin, 
				     int nbinx, int nbiny, int nbinz)
{
  int i = ibin % nbinx;
  int j = (ibin/nbinx) % nbiny;
  int k = ibin / (nbinx*nbiny);

  for (int m = 0; m < nchunk; m++)
    if (i >= chunklo[isector][m][0] && i <= chunkhi[isector][m][0] &&
	j >= chunklo[isector][m][1] && j <= chunkhi[isector][m][1] &&
	k >= chunklo[isector][m][2] && k <= chunkhi[isector][m][2]) return 1;

  return 0;
}

/* ----------------------------------------------------------------------
   setup sector chunks
   nchunk = 3 planes for dim = 3, nsector = 8
   nchunk = 4 planes for dim = 3, nsector = 4
   nchunk = 5 planes for dim = 3, nsector = 2
   nchunk = 2 lines for dim = 2, nsector = 4
   nchunk = 3 lines for dim = 2, nsector = 2
------------------------------------------------------------------------- */

void CommOffLattice::setup_sector_chunks(int nbinx, int nbiny, int nbinz)
{
  // nbinx,nbiny,nbinz are all even

  int ihalf = nbinx/2;
  int jhalf = nbiny/2;
  int khalf = nbinz/2;

  // fill in chunk boundaries for dimension = 2/3, nsector = 2/4/8

  int dimension = appoff->dimension;

  if (dimension == 2 && nsector == 4) {
    nchunk = 2;
    fill_chunk(0,0,0,ihalf,0,0,0,0);
    fill_chunk(0,1,0,0,0,jhalf,0,0);
    fill_chunk(1,0,ihalf-1,nbinx-1,0,0,0,0);
    fill_chunk(1,1,nbinx-1,nbinx-1,0,jhalf,0,0);
    fill_chunk(2,0,0,ihalf,nbiny-1,nbiny-1,0,0);
    fill_chunk(2,1,0,0,jhalf-1,nbiny-1,0,0);
    fill_chunk(3,0,ihalf-1,nbinx-1,nbiny-1,nbiny-1,0,0);
    fill_chunk(3,1,nbinx-1,nbinx-1,jhalf-1,nbiny-1,0,0);
  }

  if (dimension == 2 && nsector == 2) {
    nchunk = 3;
    fill_chunk(0,0,0,0,0,nbiny-1,0,0);
    fill_chunk(0,1,0,ihalf,0,0,0,0);
    fill_chunk(0,2,0,ihalf,nbiny-1,nbiny-1,0,0);
    fill_chunk(1,0,nbinx-1,nbinx-1,0,nbiny-1,0,0);
    fill_chunk(1,1,ihalf-1,nbinx-1,0,0,0,0);
    fill_chunk(1,2,ihalf-1,nbinx-1,nbiny-1,nbiny-1,0,0);
  }

  if (dimension == 3 && nsector == 8) {
    nchunk = 3;
    fill_chunk(0,0,0,ihalf,0,jhalf,0,0);
    fill_chunk(0,1,0,0,0,jhalf,0,khalf);
    fill_chunk(0,2,0,ihalf,0,0,0,khalf);
    fill_chunk(1,0,ihalf-1,nbinx-1,0,jhalf,0,0);
    fill_chunk(1,1,nbinx-1,nbinx-1,0,jhalf,0,khalf);
    fill_chunk(1,2,ihalf-1,nbinx-1,0,0,0,khalf);
    fill_chunk(2,0,0,ihalf,jhalf-1,nbiny-1,0,0);
    fill_chunk(2,1,0,0,jhalf-1,nbiny-1,0,khalf);
    fill_chunk(2,2,0,ihalf,nbiny-1,nbiny-1,0,khalf);
    fill_chunk(3,0,ihalf-1,nbinx-1,jhalf-1,nbiny-1,0,0);
    fill_chunk(3,1,nbinx-1,nbinx-1,jhalf-1,nbiny-1,0,khalf);
    fill_chunk(3,2,ihalf-1,nbinx-1,nbiny-1,nbiny-1,0,khalf);
    fill_chunk(4,0,0,ihalf,0,jhalf,nbinx-1,nbinz-1);
    fill_chunk(4,1,0,0,0,jhalf,khalf-1,nbinz-1);
    fill_chunk(4,2,0,ihalf,0,0,khalf-1,nbinz-1);
    fill_chunk(5,0,ihalf-1,nbinx-1,0,jhalf,nbinz-1,nbinz-1);
    fill_chunk(5,1,nbinx-1,nbinx-1,0,jhalf,khalf-1,nbinz-1);
    fill_chunk(5,2,ihalf-1,nbinx-1,0,0,khalf-1,nbinz-1);
    fill_chunk(6,0,0,ihalf,jhalf-1,nbiny-1,nbinz-1,nbinz-1);
    fill_chunk(6,1,0,0,jhalf-1,nbiny-1,khalf-1,nbinz-1);
    fill_chunk(6,2,0,ihalf,nbiny-1,nbiny-1,khalf-1,nbinz-1);
    fill_chunk(7,0,ihalf-1,nbinx-1,jhalf-1,nbiny-1,nbinz-1,nbinz-1);
    fill_chunk(7,1,nbinx-1,nbinx-1,jhalf-1,nbiny-1,khalf-1,nbinz-1);
    fill_chunk(7,2,ihalf-1,nbinx-1,nbiny-1,nbiny-1,khalf-1,nbinz-1);
  }

  if (dimension == 3 && nsector == 4) {
    nchunk = 4;
    fill_chunk(0,0,0,ihalf,0,0,0,nbinz-1);
    fill_chunk(0,1,0,0,0,jhalf,0,nbinz-1);
    fill_chunk(0,2,0,ihalf,0,jhalf,0,0);
    fill_chunk(0,3,0,ihalf,0,jhalf,nbinz-1,nbinz-1);
    fill_chunk(1,0,ihalf-1,nbinx-1,0,0,0,nbinz-1);
    fill_chunk(1,1,nbinx-1,nbinx-1,0,jhalf,0,nbinz-1);
    fill_chunk(1,2,ihalf-1,nbinx-1,0,jhalf,0,0);
    fill_chunk(1,3,ihalf-1,nbinx-1,0,jhalf,nbinz-1,nbinz-1);
    fill_chunk(2,0,0,ihalf,nbiny-1,nbiny-1,0,nbinz-1);
    fill_chunk(2,1,0,0,jhalf-1,nbiny-1,0,nbinz-1);
    fill_chunk(2,2,0,ihalf,jhalf-1,nbiny-1,0,0);
    fill_chunk(2,3,0,ihalf,jhalf-1,nbiny-1,nbinz-1,nbinz-1);
    fill_chunk(3,0,ihalf-1,nbinx-1,nbiny-1,nbiny-1,0,nbinz-1);
    fill_chunk(3,1,nbinx-1,nbinx-1,jhalf-1,nbiny-1,0,nbinz-1);
    fill_chunk(3,2,ihalf-1,nbinx-1,jhalf-1,nbiny-1,0,0);
    fill_chunk(3,3,ihalf-1,nbinx-1,jhalf-1,nbiny-1,nbinz-1,nbinz-1);
  }

  if (dimension == 3 && nsector == 2) {
    nchunk = 5;
    fill_chunk(0,0,0,0,0,nbiny-1,0,nbinz-1);
    fill_chunk(0,1,0,ihalf,0,0,0,nbinz-1);
    fill_chunk(0,2,0,ihalf,nbiny-1,nbiny-1,0,nbinz-1);
    fill_chunk(0,3,0,ihalf,0,nbiny-1,0,0);
    fill_chunk(0,4,0,ihalf,0,nbiny-1,nbinz-1,nbinz-1);
    fill_chunk(1,0,nbinx-1,nbinx-1,0,nbiny-1,0,nbinz-1);
    fill_chunk(1,1,ihalf-1,nbinx-1,0,0,0,nbinz-1);
    fill_chunk(1,3,ihalf-1,nbinx-1,nbiny-1,nbiny-1,0,nbinz-1);
    fill_chunk(1,4,ihalf-1,nbinx-1,0,nbiny-1,0,0);
    fill_chunk(1,5,ihalf-1,nbinx-1,0,nbiny-1,nbinz-1,nbinz-1);
  }
}

/* ----------------------------------------------------------------------
------------------------------------------------------------------------- */

void CommOffLattice::fill_chunk(int isector, int ichunk,
				int ilo, int ihi, int jlo, int jhi, 
				int klo, int khi)
{
  chunklo[isector][ichunk][0] = ilo;
  chunklo[isector][ichunk][1] = jlo;
  chunklo[isector][ichunk][2] = klo;
  chunkhi[isector][ichunk][0] = ihi;
  chunkhi[isector][ichunk][1] = jhi;
  chunkhi[isector][ichunk][2] = khi;
}