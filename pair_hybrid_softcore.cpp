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

#include <string.h>
#include "pair_hybrid_softcore.h"
#include "force.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

PairHybridSoftcore::PairHybridSoftcore(LAMMPS *lmp) : PairHybrid(lmp)
{
}

/* ----------------------------------------------------------------------
   modify parameters of the pair style and its sub-styles
------------------------------------------------------------------------- */

void PairHybridSoftcore::modify_params(int narg, char **arg)
{
  if (narg == 0) error->all(FLERR,"Illegal pair_modify command");

  // if 1st keyword is pair, apply other keywords to one sub-style

  if (strcmp(arg[0],"pair") == 0) {
    if (narg < 2) error->all(FLERR,"Illegal pair_modify command");
    int m;
    for (m = 0; m < nstyles; m++)
      if (strcmp(arg[1],keywords[m]) == 0) break;
    if (m == nstyles) error->all(FLERR,"Unknown pair_modify hybrid sub-style");
    int iarg = 2;

    if (multiple[m]) {
      if (narg < 3) error->all(FLERR,"Illegal pair_modify command");
      int multiflag = force->inumeric(FLERR,arg[2]);
      for (m = 0; m < nstyles; m++)
        if (strcmp(arg[1],keywords[m]) == 0 && multiflag == multiple[m]) break;
      if (m == nstyles)
        error->all(FLERR,"Unknown pair_modify hybrid sub-style");
      iarg = 3;
    }

    // if 2nd keyword (after pair) is special:
    // invoke modify_special() for the sub-style

    if (iarg < narg && strcmp(arg[iarg],"special") == 0) {
      if (narg < iarg+5)
        error->all(FLERR,"Illegal pair_modify special command");
      modify_special(m,narg-iarg,&arg[iarg+1]);
      iarg += 5;
    }

    // if 2nd keyword (after pair) is compute/tally:
    // set flag to register USER-TALLY computes accordingly

    if (iarg < narg && strcmp(arg[iarg],"compute/tally") == 0) {
      if (narg < iarg+2)
        error->all(FLERR,"Illegal pair_modify compute/tally command");
      if (strcmp(arg[iarg+1],"yes") == 0) {
        compute_tally[m] = 1;
      } else if (strcmp(arg[iarg+1],"no") == 0) {
        compute_tally[m] = 0;
      } else error->all(FLERR,"Illegal pair_modify compute/tally command");
      iarg += 2;
    }

    // apply the remaining keywords to the base pair style itself and the
    // sub-style except for "pair" and "special".
    // the former is important for some keywords like "tail" or "compute"

    if (narg-iarg > 0) {
      //Pair::modify_params(narg-iarg,&arg[iarg]);
      styles[m]->modify_params(narg-iarg,&arg[iarg]);
    }

  // apply all keywords to pair hybrid itself and every sub-style

  } else {
    Pair::modify_params(narg,arg);
    for (int m = 0; m < nstyles; m++) styles[m]->modify_params(narg,arg);
  }
}

/* ----------------------------------------------------------------------
   invoke setup for each sub-style
------------------------------------------------------------------------- */

void PairHybridSoftcore::setup()
{
  int total = 0;
  int enabled = 0;
  for (int m = 0; m < nstyles; m++) {
    styles[m]->setup();
    if (strcmp(keywords[m],"lj/cut/coul/dsf/linear") == 0) {
      total++;
      enabled += styles[m]->single_enable;
    }
  }
  if (enabled < total-1)
    error->all(FLERR,"self-energy cannot be enabled in multiple pair substyles");
}

/* ---------------------------------------------------------------------- */
