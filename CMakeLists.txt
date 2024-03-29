#
# Project configuration for an ATLAS analysis project.
#

# Set the minimum required CMake version.
cmake_minimum_required( VERSION 3.2 )

# Look up the base project, allowing any AnalysisBase-21.2.X or
# AthAnalysis-21.2.X release by default. If you want to force the usage of a
# specific analysis release version, you do that like:
#
set( base_project )
if( NOT "$ENV{AnalysisBase_DIR}" STREQUAL "" )
   find_package( AnalysisBase 21.2 REQUIRED )
   set( base_project AnalysisBase ${AnalysisBase_VERSION} )
elseif( NOT "$ENV{AthAnalysis_DIR}" STREQUAL "" )
   find_package( AthAnalysis 21.2 REQUIRED )
   set( base_project AthAnalysis ${AthAnalysis_VERSION} )
else()
   message( FATAL_ERROR "No analysis release seems to be set up!" )
endif()

# Set up CTest. It's necessary for just some technical reasons.
atlas_ctest_setup()

# Set up the analysis project from the repository.
atlas_project( WorkDir 1.0.0
   USE ${base_project} )

# Generate an environment setup script. This call makes sure that whatever
# "external" you use in your analysis project, is set up correctly in the
# runtime environment.
lcg_generate_env(  SH_FILE ${CMAKE_BINARY_DIR}/${ATLAS_PLATFORM}/env_setup.sh )
install( FILES ${CMAKE_BINARY_DIR}/${ATLAS_PLATFORM}/env_setup.sh
   DESTINATION . )

# Set up CPack. This is necessary to be able to build TGZ files correctly from
# the compiled code. Making it possible to use the code on the grid.
atlas_cpack_setup()
