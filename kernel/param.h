/*! \file param.h
 * \brief OS configuration parameters
 */

#ifndef INCLUDED_kernel_param_h
#define INCLUDED_kernel_param_h

#ifdef __cplusplus
extern "C" {
#endif


#define NPROC 64                      // maximum number of processes
#define NCPU 8                        // maximum number of CPUs
#define NOFILE 16                     // open files per process
#define NFILE 100                     // open files per system
#define NINODE 50                     // maximum number of active i-nodes
#define NDEV 10                       // maximum major device number
#define ROOTDEV 1                     // device number of file system root disk
#define INITIAL_USTACKSIZE 2 * PGSIZE // Size of initial user stack
#define MAXARG_CONSTANT                                                                            \
  128 // Chosen to make MAXARG 32 when I_USS = 4k, combination of stack aligned max string size(x) + pointer size (16 stack aligned), since this is all that's on the stack at execution start. x <= 112 in case of MAXARG args
#define MAXARG                                                                                     \
  INITIAL_USTACKSIZE / MAXARG_CONSTANT // Completely arbitrary maximum amount of args. Was 32 for 4k Stack -> MAX_ARG_CONSTANT chosen accordingly
#define MAXOPBLOCKS 10                 // max # of blocks any FS op writes
#define LOGSIZE (MAXOPBLOCKS * 3)      // max data blocks in on-disk log
#define NBUF (MAXOPBLOCKS * 12)        // size of disk block cache
#define FSSIZE 2000                    // size of file system in blocks
#define MAXPATH 128                    // maximum file path name



#ifdef __cplusplus
}
#endif

#endif
