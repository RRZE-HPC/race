cmake_minimum_required(VERSION 2.8)
include(ExternalProject)

# project name
project(RACE_example CXX C)

#set(CMAKE_CXX_STANDARD 11)

include(${PROJECT_SOURCE_DIR}/cmake/OptimizeForArchitecture.cmake)

AutodetectHostArchitecture()
OptimizeForArchitecture()


#if (NOT BUILD_SHARED_LIBS)
#    message(STATUS "No library type specified. Setting it to shared.")
#    set(BUILD_SHARED_LIBS ON CACHE BOOL "Set to ON/OFF for shared/static library build.")
#endif()

if(USE_AVX512F)
    set(VECTOR_LENGTH 8)
elseif(USE_AVX OR USE_AVX2)
    set(VECTOR_LENGTH 4)
elseif(USE_SSE2 OR USE_SSE3 OR SSE4_1 OR SSE4_2 OR SSE_4a)
    set(VECTOR_LENGTH 2)
else()
    set(VECTOR_LENGTH 1)
endif()


set(CMAKE_PREFIX_PATH ${RACE_DIR})
# find RACE
find_package(RACE REQUIRED CONFIG)
message(STATUS "Found RACE")

set(USE_LIKWID 0 CACHE BOOL "Use LIKWID for performance counter measurement")

#likwid library
if(USE_LIKWID)
    link_directories($ENV{LIKWID_LIBDIR})
    link_libraries(liblikwid.so)
    include_directories($ENV{LIKWID_INCDIR})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLIKWID_PERFMON -DLIKWID_MEASURE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIKWID_PERFMON -DLIKWID_MEASURE")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${RACE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RACE_CXX_FLAGS}")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    if (TARGET_ARCHITECTURE MATCHES "amd")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=core-avx2 -mtune=core-avx2 -save-temps -fsource-asm")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=core-avx2 -mtune=core-avx2 -save-temps -fsource-asm")
    else ()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -xHOST -save-temps -fsource-asm") #CORE-AVX2
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -xHOST -save-temps -fsource-asm")
        #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -xCORE-AVX2  -save-temps -fsource-asm")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -xCORE-AVX2  -save-temps -fsource-asm")
    endif()
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -mtune=native -Wno-write-strings")# -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -mtune=native -Wno-write-strings")# -fsanitize=address -fno-omit-frame-pointer")
endif()

find_package(OpenMP)

# TODO: would be nicer if the race config.cmake file could provide te OpenMP flags
if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

set(RACE_USE_SPMP 0 CACHE BOOL "Use RCM by SpMP for matrix preprocessing")
if (RACE_USE_SPMP)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
        set(RACE_HAVE_SPMP 1)
        set(SPMP_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        set_property(DIRECTORY PROPERTY EP_STEP_TARGETS build)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
            ExternalProject_Add(SpMP
                PREFIX ${PROJECT_BINARY_DIR}/SpMP
                URL https://github.com/christiealappatt/SpMP/archive/master.zip
                #URL https://github.com/IntelLabs/SpMP/archive/master.zip
                CONFIGURE_COMMAND ""
                BUILD_COMMAND make 
                BUILD_IN_SOURCE 1
                INSTALL_COMMAND ""
                )
        else() # GNU or Clang: Set CUSTOM_CXX variable in SpMP make
            ExternalProject_Add(SpMP
                PREFIX ${PROJECT_BINARY_DIR}/SpMP
                URL https://github.com/christiealappatt/SpMP/archive/master.zip
                #URL https://github.com/IntelLabs/SpMP/archive/master.zip
                CONFIGURE_COMMAND ""
                BUILD_COMMAND make CUSTOM_CXX=${CMAKE_CXX_COMPILER} 
                BUILD_IN_SOURCE 1
                INSTALL_COMMAND ""
                )
        endif()
    else()
        message(FATAL_ERROR "SpMP will not build with a compiler which is not Clang/GNU/Intel. Please disable SpMP or use a different compiler!")
    endif()
endif()


set(CBLAS_INCLUDE_DIR CBLAS_INCLUDE_DIR-NOTFOUND CACHE PATH "Location of *cblas.h")
# firstly, try to find a Cblas header in the default locations
if (NOT CBLAS_INCLUDE_DIR)
    message(STATUS "Try to find MKL in default paths and via MKLROOT")
    find_path(CBLAS_INCLUDE_DIR mkl_cblas.h $ENV{MKLROOT}/include)
endif()
if (NOT CBLAS_INCLUDE_DIR)
    #Have to search GSL with find_package because it will not be found by find_path
    message(STATUS "Try to find GSL in default paths and via find_package")
    find_package(GSL)
    find_path(CBLAS_INCLUDE_DIR gsl_cblas.h ${GSL_INCLUDE_DIRS}/gsl)
endif()
if (NOT CBLAS_INCLUDE_DIR)
    message(STATUS "Try to find default Cblas")
    find_path(CBLAS_INCLUDE_DIR cblas.h)
endif()

if (NOT CBLAS_INCLUDE_DIR)
    message(FATAL_ERROR "CBLAS_INCLUDE_DIR not set!")
endif()

include_directories(SYSTEM ${CBLAS_INCLUDE_DIR})

find_file(CBLAS_H_FULL NAMES "mkl_cblas.h" "cblas.h"  "gsl_cblas.h" CBLAS_H_FULL PATHS ${CBLAS_INCLUDE_DIR} PATH_SUFFIXES "cblas")
get_filename_component(CBLAS_H ${CBLAS_H_FULL} NAME)

if (${CBLAS_H} STREQUAL "mkl_cblas.h")
    message(STATUS "MKL BLAS found")
    set(RACE_HAVE_MKL 1)
    get_filename_component(MKL_LIBDIR ${CBLAS_INCLUDE_DIR}/../lib/ REALPATH)
    if (RACE_MIC_CROSSCOMPILE)
        set(MKL_PATH_SUFFIXES "mic")
    else ()
        set(MKL_PATH_SUFFIXES "intel64")
    endif()

    set(LIBMKL_CORE LIBMKL_CORE-NOTFOUND)
    set(LIBMKL_X_THREAD LIBMKL_X_THREAD-NOTFOUND)
    set(LIBMKL_INTEL_XLP64 LIBMKL_INTEL_XLP64-NOTFOUND)

    find_library(LIBMKL_CORE mkl_core PATHS ${MKL_LIBDIR} PATH_SUFFIXES ${MKL_PATH_SUFFIXES})

    if (RACE_IDX64_LOCAL)
        add_definitions(-DMKL_ILP64)
        find_library(LIBMKL_INTEL_XLP64 mkl_intel_ilp64 PATHS ${MKL_LIBDIR} PATH_SUFFIXES ${MKL_PATH_SUFFIXES})
    else()
        find_library(LIBMKL_INTEL_XLP64 mkl_intel_lp64 PATHS ${MKL_LIBDIR} PATH_SUFFIXES ${MKL_PATH_SUFFIXES})
    endif()

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel") 
        find_library(LIBMKL_X_THREAD mkl_intel_thread PATHS ${MKL_LIBDIR} PATH_SUFFIXES ${MKL_PATH_SUFFIXES})
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") 
        find_library(LIBMKL_X_THREAD mkl_gnu_thread PATHS ${MKL_LIBDIR} PATH_SUFFIXES ${MKL_PATH_SUFFIXES})
    else ()
        find_library(LIBMKL_X_THREAD mkl_gnu_thread PATHS ${MKL_LIBDIR} PATH_SUFFIXES ${MKL_PATH_SUFFIXES})
        #            message(FATAL_ERROR "OpenMP only possible with Intel or Gnu!")
    endif()

    set (BLAS_LIBRARIES "${LIBMKL_INTEL_XLP64};${LIBMKL_CORE};${LIBMKL_X_THREAD}")
    mark_as_advanced(LIBMKL_CORE LIBMKL_INTEL_XLP64 LIBMKL_X_THREAD) 
elseif (${CBLAS_H} STREQUAL "gsl_cblas.h")
    set(RACE_HAVE_GSL 1)
    get_filename_component(GSL-CONFIG_PATH ${CBLAS_INCLUDE_DIR}/../../bin REALPATH)
    list(APPEND CMAKE_PROGRAM_PATH ${GSL-CONFIG_PATH})
    find_package(GSL REQUIRED)
    message(STATUS "GSL BLAS found")
    set (BLAS_LIBRARIES "${GSL_LIBRARIES}" CACHE STRING "BLAS libraries")
else () #generic BLAS
    set (BLAS_LIBRARIES "" CACHE STRING "BLAS libraries")
    message(STATUS "Generic BLAS. BLAS_LIBRARIES has to be set manually before linking.")
endif()

set(RACE_USE_COLPACK 0 CACHE BOOL "Use ColPack for MC and ABMC")
set(RACE_USE_METIS 0 CACHE BOOL "Use METIS for ABMC")

if (RACE_USE_COLPACK)
    set(RACE_HAVE_COLPACK 1)
    set(COLPACK_INCLUDE_DIR "${COLPACK_INCLUDE_DIR}" CACHE PATH "Location of ColPack headers")
    include_directories(SYSTEM ${COLPACK_INCLUDE_DIR})
    get_filename_component(COLPACK_LIBDIR ${COLPACK_INCLUDE_DIR}/../../lib/shared_library REALPATH)

    find_library(COLPACK_LIBRARIES ColPack ${COLPACK_LIBDIR})
    if (NOT COLPACK_LIBRARIES)
        message(FATAL_ERROR "COLPACK_INCLUDE_DIR not set correctly!")
    endif()
endif()

if (RACE_USE_METIS)
    set(RACE_HAVE_METIS 1)
    set(METIS_INCLUDE_DIR "${METIS_INCLUDE_DIR}" CACHE PATH "Location of METIS headers")
    include_directories(${METIS_INCLUDE_DIR})
    get_filename_component(METIS_LIBDIR ${METIS_INCLUDE_DIR}/../lib/ REALPATH)

    find_library(METIS_LIBRARIES NAMES metis PATHS ${METIS_LIBDIR})
    if (NOT METIS_LIBRARIES)
        message(FATAL_ERROR "METIS_INCLUDE_DIR not set correctly!")
    endif()
endif()



configure_file (
    "${PROJECT_SOURCE_DIR}/include/config_eg.h.in"
    "${PROJECT_BINARY_DIR}/include/config_eg.h"
    )

#include_directories(${RACE_INCLUDE_DIR})
link_libraries(${RACE_LIBRARIES})
#set(CXX_STANDARD 11)
Set(CXX_STANDARD_REQUIRED On)
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${RACE_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR}/include)

file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/helpers/*.cpp)
add_executable(coloring ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/coloring.cpp)
add_executable(mtxPower ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/matrixPower.cpp)
add_executable(mtxPower_scan ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/matrixPower_scan.cpp)
add_executable(mtxPower_highestPower ${SOURCES} ${PROJECT_SOURCE_DIR}/src/matrixPower_highestPower.cpp)
add_executable(mtxPowerNuma ${SOURCES} ${PROJECT_SOURCE_DIR}/src/matrixPowerNuma.cpp)
add_executable(param_study ${SOURCES} ${PROJECT_SOURCE_DIR}/src/param_study.cpp)
add_executable(spmv ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/spmv.cpp)
add_executable(tradPower ${SOURCES} ${PROJECT_SOURCE_DIR}/src/tradPower.cpp)
add_executable(tradPower_highestPower ${SOURCES} ${PROJECT_SOURCE_DIR}/src/tradPower_highestPower.cpp)
add_executable(tradPower_scan ${SOURCES} ${PROJECT_SOURCE_DIR}/src/tradPower_scan.cpp)
add_executable(mklPower ${SOURCES} ${PROJECT_SOURCE_DIR}/src/mklPower.cpp)
add_executable(mklIEPower ${SOURCES} ${PROJECT_SOURCE_DIR}/src/mklIEPower.cpp)
add_executable(tradPowerNuma ${SOURCES} ${PROJECT_SOURCE_DIR}/src/tradPowerNuma.cpp)
add_executable(matStat ${SOURCES} ${PROJECT_SOURCE_DIR}/src/mat_stat.cpp)
add_executable(mklSymmSpMV ${SOURCES} ${PROJECT_SOURCE_DIR}/src/mklSymmSpMV.cpp)
add_executable(colorSymmSpMV ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/colorSymmSpMV.cpp)
add_executable(colorKACZ ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/colorKACZ.cpp)
add_executable(serialKACZ ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/serialKACZ.cpp)
add_executable(colorSymmKACZ ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/colorSymmKACZ.cpp)
add_executable(serialSymmKACZ ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/serialSymmKACZ.cpp)
add_executable(colorGS ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/colorGS.cpp)
add_executable(serialGS ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/serialGS.cpp)
add_executable(colorSymmGS ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/colorSymmGS.cpp)
add_executable(serialSymmGS ${SOURCES}  ${PROJECT_SOURCE_DIR}/src/serialSymmGS.cpp)

set(EXT_LIBRARIES "${BLAS_LIBRARIES}")

if(RACE_USE_SPMP)
    #set(EXT_LIBRARIES "${PROJECT_BINARY_DIR}/SpMP/src/SpMP/libspmp.a" "${BLAS_LIBRARIES}")
    list(APPEND EXT_LIBRARIES "${PROJECT_BINARY_DIR}/SpMP/src/SpMP/libspmp.a")
endif()

if(RACE_HAVE_COLPACK)
    list(APPEND EXT_LIBRARIES "${COLPACK_LIBRARIES}")
endif()

if(RACE_HAVE_METIS)
    list(APPEND EXT_LIBRARIES "${METIS_LIBRARIES}")
    #set(EXT_LIBRARIES "${METIS_LIBRARIES}")
endif()

target_link_libraries(coloring "${EXT_LIBRARIES}")
target_link_libraries(mtxPower "${EXT_LIBRARIES}")
target_link_libraries(mtxPower_scan "${EXT_LIBRARIES}")
target_link_libraries(mtxPower_highestPower "${EXT_LIBRARIES}")
target_link_libraries(mtxPowerNuma "${EXT_LIBRARIES}")
target_link_libraries(param_study "${EXT_LIBRARIES}")
target_link_libraries(spmv "${EXT_LIBRARIES}")
target_link_libraries(tradPower "${EXT_LIBRARIES}")
target_link_libraries(tradPower_highestPower "${EXT_LIBRARIES}")
target_link_libraries(tradPower_scan "${EXT_LIBRARIES}")
target_link_libraries(mklPower "${EXT_LIBRARIES}")
target_link_libraries(mklIEPower "${EXT_LIBRARIES}")
target_link_libraries(tradPowerNuma "${EXT_LIBRARIES}")
target_link_libraries(matStat "${EXT_LIBRARIES}")
target_link_libraries(mklSymmSpMV "${EXT_LIBRARIES}")
target_link_libraries(colorSymmSpMV "${EXT_LIBRARIES}")
target_link_libraries(colorKACZ "${EXT_LIBRARIES}")
target_link_libraries(serialKACZ "${EXT_LIBRARIES}")
target_link_libraries(colorSymmKACZ "${EXT_LIBRARIES}")
target_link_libraries(serialSymmKACZ "${EXT_LIBRARIES}")
target_link_libraries(colorGS "${EXT_LIBRARIES}")
target_link_libraries(serialGS "${EXT_LIBRARIES}")
target_link_libraries(colorSymmGS "${EXT_LIBRARIES}")
target_link_libraries(serialSymmGS "${EXT_LIBRARIES}")

if(RACE_USE_SPMP)
    include_directories(SYSTEM ${PROJECT_BINARY_DIR}/SpMP/src)
    add_dependencies(coloring SpMP)
    add_dependencies(mtxPower SpMP)
    add_dependencies(mtxPower_scan SpMP)
    add_dependencies(mtxPower_highestPower SpMP)
    add_dependencies(mtxPowerNuma SpMP)
    add_dependencies(param_study SpMP)
    add_dependencies(spmv SpMP)
    add_dependencies(tradPower SpMP)
    add_dependencies(tradPower_highestPower SpMP)
    add_dependencies(tradPower_scan SpMP)
    add_dependencies(mklPower SpMP)
    add_dependencies(mklIEPower SpMP)
    add_dependencies(tradPowerNuma SpMP)
    add_dependencies(mklSymmSpMV SpMP)
    add_dependencies(colorSymmSpMV SpMP)
    add_dependencies(colorKACZ SpMP)
    add_dependencies(serialKACZ SpMP)
    add_dependencies(colorSymmKACZ SpMP)
    add_dependencies(serialSymmKACZ SpMP)
    add_dependencies(colorGS SpMP)
    add_dependencies(serialGS SpMP)
    add_dependencies(colorSymmGS SpMP)
    add_dependencies(serialSymmGS SpMP)
endif()


file(COPY ${PROJECT_SOURCE_DIR}/matrixPower_perf_test/perf_script.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/matrixPower_perf_test/.)
file(COPY ${PROJECT_SOURCE_DIR}/matrixPower_perf_test/perf_script_w_likwid.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/matrixPower_perf_test/.)
file(COPY ${PROJECT_SOURCE_DIR}/matrixPower_perf_test/extract_matDetails.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/matrixPower_perf_test/.)
file(COPY ${PROJECT_SOURCE_DIR}/matrixPower_perf_test/check-state.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/matrixPower_perf_test/.)
file(COPY ${PROJECT_SOURCE_DIR}/matrixPower_perf_test/config.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/matrixPower_perf_test/.)
file(COPY ${PROJECT_SOURCE_DIR}/matrixPower_perf_test/machine-state.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/matrixPower_perf_test/.)


#add_library(RACE_example ${SOURCES})

#file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/*.h)
#install(FILES ${HEADERS} DESTINATION include/RACE_example)

#configure_file(${PROJECT_SOURCE_DIR}/race_example-config.cmake.in ${PROJECT_BINARY_DIR}/race_example-config.cmake @ONLY)
#install(FILES ${PROJECT_BINARY_DIR}/race_example-config.cmake DESTINATION lib/RACE_example)

#if (${BUILD_SHARED_LIBS})
#    install(TARGETS RACE_example EXPORT RACE_example-targets LIBRARY DESTINATION lib/RACE_example)
#else ()
#    install(TARGETS RACE_example EXPORT RACE_example-targets ARCHIVE DESTINATION lib/RACE_example)
#endif()


