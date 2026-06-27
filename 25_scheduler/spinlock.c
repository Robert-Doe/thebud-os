/*
 * spinlock.c — spinlock implementation body
 *
 * All logic is inlined in spinlock.h via static inline functions.
 * This file exists so the Makefile can compile spinlock.o and satisfy
 * the linker's dependency list uniformly.
 */

#include "spinlock.h"
