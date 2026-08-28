# Install script for directory: /Users/hitchedwards/assisi/build/_deps/ixwebsocket-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/libixwebsocket.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libixwebsocket.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libixwebsocket.a")
    execute_process(COMMAND "/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libixwebsocket.a")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ixwebsocket" TYPE FILE FILES
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXBase64.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXBench.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXCancellationRequest.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXConnectionState.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXDNSLookup.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXExponentialBackoff.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXGetFreePort.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXGzipCodec.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXHttp.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXHttpClient.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXHttpServer.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXNetSystem.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXProgressCallback.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterrupt.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptFactory.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptPipe.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptEvent.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSetThreadName.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSocket.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketConnect.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketFactory.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketServer.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketTLSOptions.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXStrCaseCompare.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXUdpSocket.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXUniquePtr.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXUrlParser.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXUuid.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXUtf8Validator.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXUserAgent.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocket.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketCloseConstants.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketCloseInfo.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketErrorInfo.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHandshake.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHandshakeKeyGen.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHttpHeaders.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketInitResult.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketMessage.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketMessageType.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketOpenInfo.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflate.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflateCodec.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflateOptions.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketProxyServer.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketSendData.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketSendInfo.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketServer.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketTransport.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketVersion.h"
    "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketAppleSSL.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/ixwebsocket-config.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/ixwebsocket.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake"
         "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-targets-noconfig.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/hitchedwards/assisi/build/_deps/ixwebsocket-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
