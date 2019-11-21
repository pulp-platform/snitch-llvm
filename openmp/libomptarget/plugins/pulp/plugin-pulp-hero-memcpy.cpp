/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
 *
 * Author: Alessandro Capotondi, UNIBO, (alessandro.capotondi@unibo.it)
 *
 * Copyright (C) 2005-2014 Free Software Foundation, Inc.
 *
 * This file is part of the GNU OpenMP Library (libgomp).
 *
 * Libgomp is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Under Section 7 of GPL version 3, you are granted additional
 * permissions described in the GCC Runtime Library Exception, version
 * 3.1, as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License and
 * a copy of the GCC Runtime Library Exception along with this program;
 * see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

/* Simple implementation of support routines for a shared-memory
   acc_device_host, and a non-shared memory acc_device_host_nonshm, with the
   latter built as a plugin.  */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdbool.h>
#include <vector>
#include <map>

#include "libgomp-plugin.h"
#include "gomp-constants.h"

#include "plugin-pulp-hero.h"

extern "C" const char *
GOMP_OFFLOAD_get_name (void)
{
  TRACE_FUNCTION();

  return "pulp-hero-memcpy";
}

extern "C" unsigned int
GOMP_OFFLOAD_get_caps (void)
{
  TRACE_FUNCTION();

  return GOMP_OFFLOAD_CAP_OPENMP_400;
}

extern "C" int
GOMP_OFFLOAD_hero_get_nb_rab_miss_handlers(void)
{
  TRACE_FUNCTION();

  return 0x0;
}
