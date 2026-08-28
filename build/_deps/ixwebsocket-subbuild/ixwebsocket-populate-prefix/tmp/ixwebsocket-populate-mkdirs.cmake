# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src")
  file(MAKE_DIRECTORY "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src")
endif()
file(MAKE_DIRECTORY
  "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build"
  "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix"
  "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix/tmp"
  "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix/src/ixwebsocket-populate-stamp"
  "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix/src"
  "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix/src/ixwebsocket-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix/src/ixwebsocket-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-subbuild/ixwebsocket-populate-prefix/src/ixwebsocket-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
