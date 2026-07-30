abacus_disable_feature_definitions(__MLALGO)
abacus_disable_feature_definitions(__CUDA)
abacus_disable_feature_definitions(__ROCM)
file(COPY support DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

AddTest(
  TARGET MODULE_RI_dm_mixing_test
  LIBS parameter base device
  SOURCES dm_mixing_test.cpp ../Mix_DMk_2D.cpp
)
AddTest(
  TARGET MODULE_RI_ri_cv_io_test
  LIBS base device parameter
  SOURCES ri_cv_io_test.cpp
)
AddTest(
  TARGET MODULE_RI_abfs-vector3_order
  LIBS parameter
  SOURCES abfs-vector3_order_test.cpp
)
if(ENABLE_MPI)
  add_executable(MODULE_RI_ewald_mpi_distribution_test ewald_mpi_distribution_test.cpp)
  target_link_libraries(MODULE_RI_ewald_mpi_distribution_test PRIVATE MPI::MPI_CXX)
  install(TARGETS MODULE_RI_ewald_mpi_distribution_test DESTINATION ${CMAKE_BINARY_DIR}/tests)
  add_test(
    NAME MODULE_RI_ewald_mpi_distribution_test_parallel
    COMMAND mpirun -np 4 ./MODULE_RI_ewald_mpi_distribution_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
endif()
install(DIRECTORY support DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
