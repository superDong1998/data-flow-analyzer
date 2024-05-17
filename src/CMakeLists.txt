add_llvm_library(DFGPass MODULE
    utils.h
    # graph.cpp
    DFG.cpp
)

option(CFG "Enable CFG" OFF)
option(DFG "Enable DFG" OFF)
option(INTER_ANALYSIS "Enable INTER_ANALYSIS" OFF)
option(INTRA_ANALYSIS "Enable INTRA_ANALYSIS" OFF)
option(GS_ONLY "analysis in gauss_seidel only" OFF)

if(CFG)
 add_definitions(-DCFG)
endif()

if(DFG)
 add_definitions(-DDFG)
endif()

if(INTER_ANALYSIS)
 add_definitions(-DINTER_ANALYSIS)
endif()

if(INTRA_ANALYSIS)
 add_definitions(-DINTRA_ANALYSIS)
endif()

if(GS_ONLY)
 add_definitions(-DGS_ONLY)
endif()

# target_link_libraries(DFGPass LLVM)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(DFGPass PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(DFGPass PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(DFGPass PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)