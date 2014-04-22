/* Copyright (C) 2013 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef ATOMIC_H
#define ATOMIC_H

#if defined(HAS_STDATOMIC)
#include <stdatomic.h>
#define ATOMIC_BOOL atomic_int
#define ATOMIC_LOAD(x) x
#define ATOMIC_STORE(x, y) x = y
#elif defined(HAS_GNUC_ATOMIC)
#define ATOMIC_BOOL int
#define ATOMIC_LOAD(x) __atomic_load_n(&x, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE(x, y) __atomic_store_n(&x, y, __ATOMIC_SEQ_CST)
#elif defined(HAS_CLANG_ATOMIC)
#define ATOMIC_BOOL _Atomic(int)
#define ATOMIC_LOAD(x) __c11_atomic_load(&x, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE(x, y) __c11_atomic_store(&x, y, __ATOMIC_SEQ_CST)
#else
/* As a last resort, fall back to the old, unsynchronized behaviour. This may
   fail, but a failure will only result in not using all the capabilities of the
   terminal. */
#define ATOMIC_BOOL long
#define ATOMIC_LOAD(x) x
#define ATOMIC_STORE(x, y) x = y
#endif

#endif
