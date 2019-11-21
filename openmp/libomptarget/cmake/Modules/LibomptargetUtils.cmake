#
#//===----------------------------------------------------------------------===//
#//
#// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
#// See https://llvm.org/LICENSE.txt for license information.
#// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#//
#//===----------------------------------------------------------------------===//
#

# macro to find programs on the host OS
macro( find_host_program )
  set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
  set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER )
  set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER )
  if( CMAKE_HOST_WIN32 )
   SET( WIN32 1 )
   SET( UNIX )
  elseif( CMAKE_HOST_APPLE )
   SET( APPLE 1 )
   SET( UNIX )
  endif()
  find_program( ${ARGN} )
  SET( WIN32 )
  SET( APPLE )
  SET( UNIX 1 )
  set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
  set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
  set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
endmacro()

# void libomptarget_say(string message_to_user);
# - prints out message_to_user
macro(libomptarget_say message_to_user)
  message(STATUS "LIBOMPTARGET: ${message_to_user}")
endmacro()

# void libomptarget_warning_say(string message_to_user);
# - prints out message_to_user with a warning
macro(libomptarget_warning_say message_to_user)
  message(WARNING "LIBOMPTARGET: ${message_to_user}")
endmacro()

# void libomptarget_error_say(string message_to_user);
# - prints out message_to_user with an error and exits cmake
macro(libomptarget_error_say message_to_user)
  message(FATAL_ERROR "LIBOMPTARGET: ${message_to_user}")
endmacro()
