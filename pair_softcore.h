/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifndef PAIR_CLASS

#ifndef LMP_PAIR_SOFTCORE_H
#define LMP_PAIR_SOFTCORE_H

#include "pair.h"

namespace LAMMPS_NS {

class PairSoftcore : public Pair {
 friend class FixSoftcoreEE;
 friend class ComputeSoftcoreGrid;

 public:
  PairSoftcore(class LAMMPS *);
  virtual ~PairSoftcore();
  void init_style();
  void modify_params(int narg, char **arg);
  void write_restart(FILE *);
  void read_restart(FILE *);

 protected:
  double alpha, exponent_n, exponent_p;  // softcore model parameters
  double lambda;                         // coupling parameter value
  int    gridflag;    // 1 if grid must be computed in the current step
  int    uptodate;    // 1 if grid was computed in the latest step
  int    gridsize;    // number of nodes in the grid
  double *lambdanode; // lambda value at each node
  double *evdwlnode;  // total pairwise interaction energy at each node
  double *etailnode;  // tail correction for energy at each node
  double *ecoulnode;  // total pairwise interaction energy at each node Added by ANa
  void allocate();
  void add_node_to_grid(double);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

E: Pair cutoff < Respa interior cutoff

One or more pairwise cutoffs are too short to use with the specified
rRESPA cutoffs.

*/
