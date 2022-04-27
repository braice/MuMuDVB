/*
 * Module: semaphore.h
 *
 * Purpose:
 *	Semaphores aren't actually part of the PThreads standard.
 *	They are defined by the POSIX Standard:
 *
 *		POSIX 1003.1b-1993	(POSIX.1b)
 *
 * --------------------------------------------------------------------------
 *
 *      pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999-2021 pthreads-win32 / pthreads4w contributors
 *
 *      Homepage1: http://sourceware.org/pthreads-win32/
 *      Homepage2: http://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 * 
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 * 
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 * 
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 * --------------------------------------------------------------------------
 */
#pragma once
#if !defined( SEMAPHORE_H )
#define SEMAPHORE_H

/* FIXME: POSIX.1 says that _POSIX_SEMAPHORES should be defined
 * in <unistd.h>, not here; for later POSIX.1 versions, its value
 * should match the corresponding _POSIX_VERSION number, but in
 * the case of POSIX.1b-1993, the value is unspecified.
 *
 * Notwithstanding the above, since POSIX semaphores, (and indeed
 * having any <unistd.h> to #include), are not a standard feature
 * on MS-Windows, it is convenient to retain this definition here;
 * we may consider adding a hook, to make it selectively available
 * for inclusion by <unistd.h>, in those cases (e.g. MinGW) where
 * <unistd.h> is provided.
 */
#define _POSIX_SEMAPHORES

/* Internal macros, common to the public interfaces for various
 * pthreads-win32 components, are defined in <_ptw32.h>; we must
 * include them here.
 */
#include "_ptw32.h"

/* The sem_timedwait() function was added in POSIX.1-2001; it
 * requires struct timespec to be defined, at least as a partial
 * (a.k.a. incomplete) data type.  Forward declare it as such,
 * then include <time.h> selectively, to acquire a complete
 * definition, (if available).
 */
struct timespec;
#define __need_struct_timespec
#include <time.h>

/* The data type used to represent our semaphore implementation,
 * as required by POSIX.1; FIXME: consider renaming the underlying
 * structure tag, to avoid possible pollution of user namespace.
 */
typedef struct sem_t_ * sem_t;

/* POSIX.1b (and later) mandates SEM_FAILED as the value to be
 * returned on failure of sem_open(); (our implementation is a
 * stub, which will always return this).
 */
#define SEM_FAILED  (sem_t *)(intptr_t)(-1)

PTW32_BEGIN_C_DECLS

/* Function prototypes: some are implemented as stubs, which
 * always fail; (FIXME: identify them).
 */
PTW32_DLLPORT int PTW32_CDECL sem_init (sem_t * sem,
					int pshared,
					unsigned int value);

PTW32_DLLPORT int PTW32_CDECL sem_destroy (sem_t * sem);

PTW32_DLLPORT int PTW32_CDECL sem_trywait (sem_t * sem);

PTW32_DLLPORT int PTW32_CDECL sem_wait (sem_t * sem);

PTW32_DLLPORT int PTW32_CDECL sem_timedwait (sem_t * sem,
					     const struct timespec * abstime);

PTW32_DLLPORT int PTW32_CDECL sem_post (sem_t * sem);

PTW32_DLLPORT int PTW32_CDECL sem_post_multiple (sem_t * sem,
						 int count);

PTW32_DLLPORT sem_t * PTW32_CDECL sem_open (const char * name, int oflag, ...);

PTW32_DLLPORT int PTW32_CDECL sem_close (sem_t * sem);

PTW32_DLLPORT int PTW32_CDECL sem_unlink (const char * name);

PTW32_DLLPORT int PTW32_CDECL sem_getvalue (sem_t * sem,
					    int * sval);

PTW32_END_C_DECLS

#endif				/* !SEMAPHORE_H */
