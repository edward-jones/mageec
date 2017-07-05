cmake_minimum_required(VERSION 3.0.2)
project(LLVM_FEATURE_EXTRACT_PLUGIN)


option (LLVM_INSTALL_DIR
        "Directory containing the LLVM install.")
option (MAGEEC_INSTALL_DIR
        "Directory containing the MAGEEC install.")


set (MAGEEC_VERSION_MAJOR 2)
set (MAGEEC_VERSION_MINOR 0)
set (MAGEEC_VERSION_PATCH 0)



if (LLVM_INSTALL_DIR)
  set (LLVM_INCLUDE_DIR ${LLVM_INSTALL_DIR}/include)
  set (LLVM_LIB_DIR ${LLVM_INSTALL_DIR}/lib)

  include_directories(${LLVM_INCLUDE_DIR})
  link_directories(${LLVM_LIB_DIR})
endif ()


if (MAGEEC_INSTALL_DIR)
  set (MAGEEC_INCLUDE_DIR ${MAGEEC_INSTALL_DIR}/include)
  set (MAGEEC_LIB_DIR ${MAGEEC_INSTALL_DIR}/lib)

  include_directories(${MAGEEC_INCLUDE_DIR})
  link_directories(${MAGEEC_LIB_DIR})
endif()


set (CMAKE_CXX_FLAGS "-g3 -std=c++11")
add_definitions (
  -DMAGEEC_VERSION_MAJOR=2
  -DMAGEEC_VERSION_MINOR=0
  -DMAGEEC_VERSION_PATCH=0
)


add_library(llvm_feature_extract SHARED
  FeatureExtract.cpp
)
target_link_libraries(llvm_feature_extract LLVM mageec)

install(TARGETS llvm_feature_extract
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
