/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(hybrid/softcore,PairHybridSoftcore)

#else

#ifndef LMP_PAIR_HYBRID_SOFTCORE_H
#define LMP_PAIR_HYBRID_SOFTCORE_H

#include <stdio.h>
#include "pair_hybrid.h"

namespace LAMMPS_NS {

class PairHybridSoftcore : public PairHybrid {
 friend class FixSoftcoreEE;
 friend class ComputeSoftcoreGrid;

 public:
  PairHybridSoftcore(class LAMMPS *);
  void modify_params(int narg, char **arg);
  void setup();
};

}

#endif
#endif
