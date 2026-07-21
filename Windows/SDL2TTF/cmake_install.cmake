# Install script for directory: C:/Repos/SDL_ttf

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/SDL2_ttf")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
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

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Repos/spikedão/Windows/SDL2TTF/Debug/SDL2_ttfd.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Repos/spikedão/Windows/SDL2TTF/Release/SDL2_ttf.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Repos/spikedão/Windows/SDL2TTF/MinSizeRel/SDL2_ttf.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Repos/spikedão/Windows/SDL2TTF/RelWithDebInfo/SDL2_ttf.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "library" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Repos/spikedão/Windows/SDL2TTF/Debug/SDL2_ttfd.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Repos/spikedão/Windows/SDL2TTF/Release/SDL2_ttf.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Repos/spikedão/Windows/SDL2TTF/MinSizeRel/SDL2_ttf.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Repos/spikedão/Windows/SDL2TTF/RelWithDebInfo/SDL2_ttf.dll")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SDL2" TYPE FILE FILES "C:/Repos/SDL_ttf/SDL_ttf.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES
    "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttfConfig.cmake"
    "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttfConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES
    "C:/Repos/SDL_ttf/cmake/PkgConfigHelper.cmake"
    "C:/Repos/SDL_ttf/cmake/Findharfbuzz.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2_ttf-shared-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2_ttf-shared-targets.cmake"
         "C:/Repos/spikedão/Windows/SDL2TTF/CMakeFiles/Export/272ceadb8458515b2ae4b5630a6029cc/SDL2_ttf-shared-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2_ttf-shared-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2_ttf-shared-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "C:/Repos/spikedão/Windows/SDL2TTF/CMakeFiles/Export/272ceadb8458515b2ae4b5630a6029cc/SDL2_ttf-shared-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "C:/Repos/spikedão/Windows/SDL2TTF/CMakeFiles/Export/272ceadb8458515b2ae4b5630a6029cc/SDL2_ttf-shared-targets-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "C:/Repos/spikedão/Windows/SDL2TTF/CMakeFiles/Export/272ceadb8458515b2ae4b5630a6029cc/SDL2_ttf-shared-targets-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "C:/Repos/spikedão/Windows/SDL2TTF/CMakeFiles/Export/272ceadb8458515b2ae4b5630a6029cc/SDL2_ttf-shared-targets-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "C:/Repos/spikedão/Windows/SDL2TTF/CMakeFiles/Export/272ceadb8458515b2ae4b5630a6029cc/SDL2_ttf-shared-targets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "devel" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    
        # FIXME: use file(COPY_FILE) if CMake 3.21+
        execute_process(COMMAND "C:/Repos/CMake/bin/cmake.exe" -E copy_if_different
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf-Debug.pc"
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
        file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
            TYPE FILE
            FILES "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    
        # FIXME: use file(COPY_FILE) if CMake 3.21+
        execute_process(COMMAND "C:/Repos/CMake/bin/cmake.exe" -E copy_if_different
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf-Release.pc"
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
        file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
            TYPE FILE
            FILES "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    
        # FIXME: use file(COPY_FILE) if CMake 3.21+
        execute_process(COMMAND "C:/Repos/CMake/bin/cmake.exe" -E copy_if_different
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf-MinSizeRel.pc"
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
        file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
            TYPE FILE
            FILES "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    
        # FIXME: use file(COPY_FILE) if CMake 3.21+
        execute_process(COMMAND "C:/Repos/CMake/bin/cmake.exe" -E copy_if_different
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf-RelWithDebInfo.pc"
            "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
        file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
            TYPE FILE
            FILES "C:/Repos/spikedão/Windows/SDL2TTF/SDL2_ttf.pc")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "library" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/licenses/SDL2_ttf" TYPE FILE FILES "C:/Repos/SDL_ttf/LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/Repos/spikedão/Windows/SDL2TTF/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
