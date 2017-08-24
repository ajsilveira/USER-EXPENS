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

#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pair_lj_cut_coul_dsf_linear.h"
#include "atom.h"
#include "comm.h"
#include "force.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "memory.h"
#include "math_const.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace MathConst;

#define EWALD_F   1.12837917
#define EWALD_P   0.3275911
#define A1        0.254829592
#define A2       -0.284496736
#define A3        1.421413741
#define A4       -1.453152027
#define A5        1.061405429
#define two_pis   1.12837916709551

/* ---------------------------------------------------------------------- */

PairLJCutCoulDSFLinear::PairLJCutCoulDSFLinear(LAMMPS *lmp) : Pair(lmp)
{
  single_enable = 0;
  self_flag = 1;
  unshifted_flag = 3;
}

/* ---------------------------------------------------------------------- */

PairLJCutCoulDSFLinear::~PairLJCutCoulDSFLinear()
{
  if (!copymode) {
    if (allocated) {
      memory->destroy(setflag);
      memory->destroy(cutsq);

      memory->destroy(cut_lj);
      memory->destroy(cut_ljsq);
      memory->destroy(epsilon);
      memory->destroy(sigma);
      memory->destroy(lj1);
      memory->destroy(lj2);
      memory->destroy(lj3);
      memory->destroy(lj4);
      memory->destroy(offset);
    }
  }
}

/* ---------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;
  double qtmp,xtmp,ytmp,ztmp,delx,dely,delz,vr,fr,evdwl,ecoul,fpair;
  double r,rsq,r2inv,r6inv,factor_coul,factor_lj,prefactor;
  int *ilist,*jlist,*numneigh,**firstneigh;
  
  evdwl = ecoul = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;
  
  double **x = atom->x;
  double **f = atom->f;
  double *q = atom->q;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  double *special_lj = force->special_lj;
  double *special_coul = force->special_coul;
  int newton_pair = force->newton_pair;
  double qqrd2e = force->qqrd2e;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // Compute self energy:
  if (eflag && self_flag)
    for (i = 0; i < nlocal; i++) {
      qtmp = qqrd2e*q[i];
      ev_tally(i,i,nlocal,0,0.0,e_self*qtmp*qtmp,0.0,0.0,0.0,0.0);
    }

  // loop over neighbors of my atoms

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    qtmp = qqrd2e*q[i];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    itype = type[i];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor_lj = special_lj[sbmask(j)];
      factor_coul = special_coul[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      jtype = type[j];

      if (rsq < cutsq[itype][jtype]) {
        r2inv = 1.0/rsq;
        fpair = 0.0;
        if (rsq < cut_ljsq[itype][jtype]) {
          r6inv = r2inv*r2inv*r2inv;
          fpair += factor_lj*r6inv*(lj1[itype][jtype]*r6inv - lj2[itype][jtype]);
        }
        if (rsq < cut_coulsq) {
          r = sqrt(rsq);
          (this->*unshifted)( r, vr, fr );
          prefactor = factor_coul*qtmp*q[j];
          fpair += prefactor*(fr - f_shift)*r;
        }

        fpair *= r2inv;
        f[i][0] += delx*fpair;
        f[i][1] += dely*fpair;
        f[i][2] += delz*fpair;
        if (newton_pair || j < nlocal) {
          f[j][0] -= delx*fpair;
          f[j][1] -= dely*fpair;
          f[j][2] -= delz*fpair;
        }

        if (eflag) {
          if (rsq < cut_ljsq[itype][jtype])
            evdwl = factor_lj*r6inv*(lj3[itype][jtype]*r6inv-lj4[itype][jtype]) -
                    offset[itype][jtype];
          else
            evdwl = 0.0;

          if (rsq < cut_coulsq)
            ecoul = prefactor*(vr + r*f_shift - e_shift);
          else
            ecoul = 0.0;
        }

        if (evflag) ev_tally(i,j,nlocal,newton_pair,
                             evdwl,ecoul,fpair,delx,dely,delz);
      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n+1,n+1,"pair:cutsq");

  memory->create(cut_lj,n+1,n+1,"pair:cut_lj");
  memory->create(cut_ljsq,n+1,n+1,"pair:cut_ljsq");
  memory->create(epsilon,n+1,n+1,"pair:epsilon");
  memory->create(sigma,n+1,n+1,"pair:sigma");
  memory->create(lj1,n+1,n+1,"pair:lj1");
  memory->create(lj2,n+1,n+1,"pair:lj2");
  memory->create(lj3,n+1,n+1,"pair:lj3");
  memory->create(lj4,n+1,n+1,"pair:lj4");
  memory->create(offset,n+1,n+1,"pair:offset");
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::settings(int narg, char **arg)
{
  if (narg < 2) error->all(FLERR,"Illegal pair_style command");

  alpha = force->numeric(FLERR,arg[0]);
  cut_lj_global = force->numeric(FLERR,arg[1]);

  int iarg = 2;
  if (narg < 3)
    cut_coul = cut_lj_global;
  else if ( strcmp(arg[2],"self") == 0 || strcmp(arg[2],"damp") == 0 )
    cut_coul = cut_lj_global;
  else {
    cut_coul = force->numeric(FLERR,arg[2]);
    iarg++;
  }

  // Check keywords:
  while (iarg < narg) {
    if (strcmp(arg[iarg],"self") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style command");
      if      (strcmp(arg[iarg+1],"yes") == 0) self_flag = 1;
      else if (strcmp(arg[iarg+1],"no" ) == 0) self_flag = 0;
      else error->all(FLERR,"Illegal pair_style command");
      iarg += 2;
    }
    else if (strcmp(arg[iarg],"damp") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style command");
      if      (strcmp(arg[iarg+1],"none" ) == 0) unshifted_flag = 0;
      else if (strcmp(arg[iarg+1],"debye") == 0) unshifted_flag = 1;
      else if (strcmp(arg[iarg+1],"gauss") == 0) unshifted_flag = 2;
      else if (strcmp(arg[iarg+1],"erfc" ) == 0) unshifted_flag = 3;
      else error->all(FLERR,"Illegal pair_style command");
      iarg += 2;
    }
    else error->all(FLERR,"Illegal pair_style command");
  }
  single_enable = !self_flag;

  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++)
      for (j = i+1; j <= atom->ntypes; j++)
        if (setflag[i][j])
          cut_lj[i][j] = cut_lj_global;
  }
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::coeff(int narg, char **arg)
{
  if (narg < 4 || narg > 5) 
    error->all(FLERR,"Incorrect args for pair coefficients");
  if (!allocated) allocate();

  int ilo,ihi,jlo,jhi;
  force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
  force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);

  double epsilon_one = force->numeric(FLERR,arg[2]);
  double sigma_one = force->numeric(FLERR,arg[3]);

  double cut_lj_one = cut_lj_global;
  if (narg == 5) cut_lj_one = force->numeric(FLERR,arg[4]);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j <= jhi; j++) {
      epsilon[i][j] = epsilon_one;
      sigma[i][j] = sigma_one;
      cut_lj[i][j] = cut_lj_one;
      setflag[i][j] = 1;
      count++;
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients");
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::init_style()
{
  if (!atom->q_flag)
    error->all(FLERR,"Pair style lj/cut/coul/dsf requires atom charges");

  neighbor->request(this,instance_me);

  init_parameters();
}

/* ----------------------------------------------------------------------
   init pair style parameters
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::init_parameters()
{
  if (unshifted_flag == 0)
    unshifted = &PairLJCutCoulDSFLinear::unshifted_none;
  else if (unshifted_flag == 1)
    unshifted = &PairLJCutCoulDSFLinear::unshifted_debye;
  else if (unshifted_flag == 2)
    unshifted = &PairLJCutCoulDSFLinear::unshifted_gauss;
  else if (unshifted_flag == 3)
    unshifted = &PairLJCutCoulDSFLinear::unshifted_erfc;

  cut_coulsq = cut_coul * cut_coul;
  (this->*unshifted)( cut_coul, e_shift, f_shift );
  e_shift += f_shift*cut_coul;

  if (unshifted_flag == 1)
    e_self = -0.5*(e_shift + alpha)/force->qqrd2e;
  else if (unshifted_flag == 3)
    e_self = -0.5*(e_shift + two_pis*alpha)/force->qqrd2e;
  else
    e_self = -0.5*e_shift/force->qqrd2e;
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairLJCutCoulDSFLinear::init_one(int i, int j)
{
  if (setflag[i][j] == 0) {
    epsilon[i][j] = mix_energy(epsilon[i][i],epsilon[j][j],
                               sigma[i][i],sigma[j][j]);
    sigma[i][j] = mix_distance(sigma[i][i],sigma[j][j]);
    cut_lj[i][j] = mix_distance(cut_lj[i][i],cut_lj[j][j]);
  }

  double cut = MAX(cut_lj[i][j],cut_coul);
  cut_ljsq[i][j] = cut_lj[i][j] * cut_lj[i][j];
  
  lj1[i][j] = 48.0 * epsilon[i][j] * pow(sigma[i][j],12.0);
  lj2[i][j] = 24.0 * epsilon[i][j] * pow(sigma[i][j],6.0);
  lj3[i][j] = 4.0 * epsilon[i][j] * pow(sigma[i][j],12.0);
  lj4[i][j] = 4.0 * epsilon[i][j] * pow(sigma[i][j],6.0);
     
  if (offset_flag) {
    double ratio = sigma[i][j] / cut_lj[i][j];
    offset[i][j] = 4.0 * epsilon[i][j] * (pow(ratio,12.0) - pow(ratio,6.0));
  } else offset[i][j] = 0.0;
  
  cut_ljsq[j][i] = cut_ljsq[i][j];
  lj1[j][i] = lj1[i][j];
  lj2[j][i] = lj2[i][j];
  lj3[j][i] = lj3[i][j];
  lj4[j][i] = lj4[i][j];
  offset[j][i] = offset[i][j];

  // compute I,J contribution to long-range tail correction
  // count total # of atoms of type I and J via Allreduce

  if (tail_flag) {
    int *type = atom->type;
    int nlocal = atom->nlocal;

    double count[2],all[2];
    count[0] = count[1] = 0.0;
    for (int k = 0; k < nlocal; k++) {
      if (type[k] == i) count[0] += 1.0;
      if (type[k] == j) count[1] += 1.0;
    }
    MPI_Allreduce(count,all,2,MPI_DOUBLE,MPI_SUM,world);

    double sig2 = sigma[i][j]*sigma[i][j];
    double sig6 = sig2*sig2*sig2;
    double rc3 = cut_lj[i][j]*cut_lj[i][j]*cut_lj[i][j];
    double rc6 = rc3*rc3;
    double rc9 = rc3*rc6;
    etail_ij = 8.0*MY_PI*all[0]*all[1]*epsilon[i][j] * 
               sig6 * (sig6 - 3.0*rc6) / (9.0*rc9); 
    ptail_ij = 16.0*MY_PI*all[0]*all[1]*epsilon[i][j] * 
               sig6 * (2.0*sig6 - 3.0*rc6) / (9.0*rc9); 
  } 

  return cut;
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::write_restart(FILE *fp)
{
  write_restart_settings(fp);

  int i,j;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        fwrite(&epsilon[i][j],sizeof(double),1,fp);
        fwrite(&sigma[i][j],sizeof(double),1,fp);
        fwrite(&cut_lj[i][j],sizeof(double),1,fp);
      }
    }
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::read_restart(FILE *fp)
{
  read_restart_settings(fp);
  allocate();

  int i,j;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) fread(&setflag[i][j],sizeof(int),1,fp);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
      if (setflag[i][j]) {
        if (me == 0) {
          fread(&epsilon[i][j],sizeof(double),1,fp);
          fread(&sigma[i][j],sizeof(double),1,fp);
          fread(&cut_lj[i][j],sizeof(double),1,fp);
        }
        MPI_Bcast(&epsilon[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&sigma[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut_lj[i][j],1,MPI_DOUBLE,0,world);
      }
    }
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::write_restart_settings(FILE *fp)
{
  fwrite(&alpha,sizeof(double),1,fp);
  fwrite(&cut_lj_global,sizeof(double),1,fp);
  fwrite(&cut_coul,sizeof(double),1,fp);
  fwrite(&offset_flag,sizeof(int),1,fp);
  fwrite(&mix_flag,sizeof(int),1,fp);
  fwrite(&tail_flag,sizeof(int),1,fp);
  fwrite(&self_flag,sizeof(int),1,fp);
  fwrite(&unshifted_flag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    fread(&alpha,sizeof(double),1,fp);
    fread(&cut_lj_global,sizeof(double),1,fp);
    fread(&cut_coul,sizeof(double),1,fp);
    fread(&offset_flag,sizeof(int),1,fp);
    fread(&mix_flag,sizeof(int),1,fp);
    fread(&tail_flag,sizeof(int),1,fp);
    fread(&self_flag,sizeof(int),1,fp);
    fread(&unshifted_flag,sizeof(int),1,fp);
  }
  MPI_Bcast(&alpha,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&cut_lj_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&cut_coul,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&offset_flag,1,MPI_INT,0,world);
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world);
  MPI_Bcast(&tail_flag,1,MPI_INT,0,world);
  MPI_Bcast(&self_flag,1,MPI_INT,0,world);
  MPI_Bcast(&unshifted_flag,1,MPI_INT,0,world);
}

/* ---------------------------------------------------------------------- */

double PairLJCutCoulDSFLinear::single(int i, int j, int itype, int jtype, double rsq,
                                double factor_coul, double factor_lj,
                                double &fforce)
{
  double r2inv,r6inv,r,vr,fr,prefactor;

  r2inv = 1.0/rsq;
  fforce = 0.0;
  if (rsq < cut_ljsq[itype][jtype]) {
    r6inv = r2inv*r2inv*r2inv;
    fforce += factor_lj*r6inv*(lj1[itype][jtype]*r6inv - lj2[itype][jtype]);
  }
  if (rsq < cut_coulsq) {
    r = sqrt(rsq);
    (this->*unshifted)( r, vr, fr );
    prefactor = factor_coul * force->qqrd2e * atom->q[i] * atom->q[j];
    fforce += prefactor*(fr-f_shift)*r;
  }
  fforce *= r2inv;

  double eng = 0.0;
  if (rsq < cut_ljsq[itype][jtype])
    eng += factor_lj*r6inv*(lj3[itype][jtype]*r6inv-lj4[itype][jtype]) -
           offset[itype][jtype];
  if (rsq < cut_coulsq)
    eng += prefactor * (vr + r*f_shift - e_shift);
 
  return eng;
}

/* ---------------------------------------------------------------------- */

void *PairLJCutCoulDSFLinear::extract(const char *str, int &dim)
{
  if (strcmp(str,"cut_coul") == 0) {
    dim = 0;
    return (void *) &cut_coul;
  }
  return NULL;
}

/* ---------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::unshifted_none( double r, double &v, double &f )
{
  v = 1.0/r;
  f = v*v;
}

/* ---------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::unshifted_debye( double r, double &v, double &f )
{
  double ar = alpha*r;
  f = 1.0/r;
  v = exp(-ar)*f;
  f *= (1.0 + ar)*v;
}

/* ---------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::unshifted_gauss( double r, double &v, double &f )
{
  double ar = alpha*r;
  ar *= ar;
  f = 1.0/r;
  v = exp(-ar)*f;
  f *= (1.0 + 2.0*ar)*v;
}

/* ---------------------------------------------------------------------- */

void PairLJCutCoulDSFLinear::unshifted_erfc( double r, double &v, double &f )
{
  double ar = alpha*r;
  f = exp(-ar*ar)/r;
  v = 1.0 / (1.0 + EWALD_P*ar);
  v *= (A1 + v*(A2 + v*(A3 + v*(A4 + v*A5))))*f;
  f = v/r + two_pis*alpha*f;
}
