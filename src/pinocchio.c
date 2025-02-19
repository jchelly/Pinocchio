/*****************************************************************
 *                        PINOCCHIO  V4.1                        *
 *  (PINpointing Orbit-Crossing Collapsed HIerarchical Objects)  *
 *****************************************************************
 
 This code was written by
 Pierluigi Monaco
 Copyright (C) 2016
 
 web page: http://adlibitum.oats.inaf.it/monaco/pinocchio.html
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "pinocchio.h"

void abort_code(void);
void write_cputimes(void);

int main(int argc, char **argv, char **envp)
{
  double time;
  int ThisGrid;

  /* Initialize MPI */
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
  MPI_Comm_size(MPI_COMM_WORLD, &NTasks);

  /* timing of the code */
  cputime.total=MPI_Wtime();

  if (!ThisTask)
    {
      printf("[%s] This is pinocchio V4.1, running on %d MPI tasks\n\n",fdate(),NTasks);
#ifdef TWO_LPT
#ifndef THREE_LPT
      printf("This version uses 2LPT displacements\n");
#else
      printf("This version uses 3LPT displacements\n");
#endif
#else
      printf("This version uses Zeldovich displacements\n");
#endif
#ifdef ROTATE_BOX
      printf("The output will be rotated to reproduce N-GenIC and 2LPTic orientation\n");
#endif
#ifdef WHITENOISE
      printf("Initial conditions will be read from a white noise file\n");
#endif
#ifdef SCALE_DEPENDENT_GROWTH
      printf("This version of the code works with scale-dependent growing modes.\n");
      printf("These will be computed from CAMB tables\n");
#ifdef THREE_LPT
      if (!ThisTask)
	{
	  printf("********************************************************\n");
	  printf("Scale-dependent growth presently does not work with 3LPT\n");
	  printf("Please recompile without the THREE_LPT directive\n");
	  printf("********************************************************\n");
	}
#endif
#endif
#ifdef NO_RANDOM_MODULES
      printf("Initial conditions will be generated with non-random modules of the Fourier modes\n");
#endif
#ifdef LIST_OF_FRAGMENT_PARAMETERS
      printf("Fragmentation will be repeated many times, reading parameters from list_of_fragment_parameters.txt\n");
      printf("NB: THIS OPTION IS USEFUL ONLY TO FINE-TUNE THE MASS FUNCTION\n");
#endif
#ifdef SMOOTH_VELOCITIES
      printf("Velocities will be computed at the same smoothing radius as the collapse time.\n");
      printf("NB: THIS IS NOT A RECOMMENDED OPTION, USE IT IF YOU KNOW WHAT YOU ARE DOING!\n");
#endif

    }

  /* checks that the parameter file is given in the command line */
  if (argc<2)
    {
      if (!ThisTask)
	printf("Usage: pinocchio.x parameterfile\n");
      MPI_Finalize();
      return 0;
    }

  /* initialization */
  memset(&params, 0, sizeof(param_data));
  strcpy(params.ParameterFile,argv[1]);
  if (initialization())
    abort_code();


  fflush(stdout);
  MPI_Barrier(MPI_COMM_WORLD);

  /* called as "pinocchio.x parameterfile 1" it writes the density field  of Grid 0
     in configuration space and exits */
  if (argc>=3 && atoi(argv[2])==1)
    {
      if (!ThisTask)
	{
	  printf("In this configuration pinocchio will only write the linear density field\n");
	  printf("in the Data/ directory\n");
	}

      for (ThisGrid=0; ThisGrid<Ngrids; ThisGrid++)
	{
	  write_in_cvector(ThisGrid, kdensity[ThisGrid]);
	  time=reverse_transform(ThisGrid);
	  if (!ThisTask)
	    printf("[%s] compute_derivative: done fft, cpu time = %f\n",fdate(),time);
	  write_from_rvector(ThisGrid, density[ThisGrid]);
	  if (write_density(ThisGrid))
	    abort_code();
	}
      if (!ThisTask)
	printf("Pinocchio done!\n");
      MPI_Finalize();

      return 0;
    }

  /* called as "pinocchio.x parameterfile 2" it computes and writes displacement fields, then exit */
  if (argc>=3 && atoi(argv[2])==2)
    {
      if (!ThisTask)
	{
	  printf("In this configuration pinocchio will only produce a GADGET snapshot\n");
	  printf("at the first redshift specified by the %s file (z=%f)\n",
		 params.OutputList,outputs.z[0]);
	}


#ifdef THREE_LPT
      if (!ThisTask)
	{
	  printf("*********************************************************\n");
	  printf("Writing of LPT snapshot presently does not work with 3LPT\n");
	  printf("Please recompile without the THREE_LPT directive\n");
	  printf("*********************************************************\n");
	}
#else

      if (compute_displacements())
        abort_code();

      if (write_LPT_snapshot(outputs.z[0]))
        abort_code();

#endif
	
      if (!ThisTask)
	printf("Pinocchio done!\n");
      MPI_Finalize();

      return 0;
    }

  /* computation of collapse times and displacements */
  if (compute_fmax())
    abort_code();

  fflush(stdout);
  MPI_Barrier(MPI_COMM_WORLD);

  /* fragmentation of the collapsed medium */
  if (fragment())
    abort_code();

  fflush(stdout);
  MPI_Barrier(MPI_COMM_WORLD);

  /* output detailed cpu times */
  cputime.total=MPI_Wtime()-cputime.total;
  if (!ThisTask)
    write_cputimes();

  /* done */
  if (!ThisTask)
    printf("Pinocchio done!\n");
  MPI_Finalize();

  return 0;
}


void abort_code(void)
{
  printf("Task %d aborting...\n",ThisTask);
  MPI_Abort(MPI_COMM_WORLD,1);
}


void write_cputimes()
{
  printf("Total:            %14.6f\n",cputime.total);
  printf("Initialization:   %14.6f (%5.2f%%)\n",cputime.init, 100.*cputime.init/cputime.total);
  printf("  Density in FS:  %14.6f (%5.2f%%)\n",cputime.dens, 100.*cputime.dens/cputime.total);
  printf("fmax:             %14.6f (%5.2f%%)\n",cputime.fmax, 100.*cputime.fmax /cputime.total);
#ifdef TWO_LPT
  printf("  LPT:            %14.6f (%5.2f%%)\n",cputime.lpt,  100.*cputime.lpt  /cputime.total);
#endif
  printf("  FFTs:           %14.6f (%5.2f%%)\n",cputime.fft,  100.*cputime.fft  /cputime.total);
  printf("  Collapse times: %14.6f (%5.2f%%)\n",cputime.coll, 100.*cputime.coll /cputime.total);
  printf("  Velocities:     %14.6f (%5.2f%%)\n",cputime.vel,  100.*cputime.vel  /cputime.total);
  printf("Fragmentation:    %14.6f (%5.2f%%)\n",cputime.frag, 100.*cputime.frag /cputime.total);
  printf("  Redistribution: %14.6f (%5.2f%%)\n",cputime.distr,100.*cputime.distr/cputime.total);
  printf("  Sorting:        %14.6f (%5.2f%%)\n",cputime.sort, 100.*cputime.sort /cputime.total);
#ifdef PLC
  printf("  Groups total:   %14.6f (%5.2f%%)\n",cputime.group,100.*cputime.group/cputime.total);
  printf("  Groups PLC:     %14.6f (%5.2f%%)\n",cputime.plc,100.*cputime.plc/cputime.total);
#else
  printf("  Groups:         %14.6f (%5.2f%%)\n",cputime.group,100.*cputime.group/cputime.total);
#endif
  printf("Total I/O:        %14.6f (%5.2f%%)\n",cputime.io,   100.*cputime.io   /cputime.total);
}
