cmake_minimum_required(VERSION 3.0.0)
project(tdms_dump_structure VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(tdms_dump_structure tdms_dump_structure/tdms_dump_structure.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_test(NAME dump_step1 COMMAND tdms_dump_structure ${CMAKE_SOURCE_DIR}/tdms_example_files/tdms-file-format-internal-structure/IncrementalMetaInformationExample_step1.tdms)
add_test(NAME dump_step2 COMMAND tdms_dump_structure ${CMAKE_SOURCE_DIR}/tdms_example_files/tdms-file-format-internal-structure/IncrementalMetaInformationExample_step2.tdms)
add_test(NAME dump_step3 COMMAND tdms_dump_structure ${CMAKE_SOURCE_DIR}/tdms_example_files/tdms-file-format-internal-structure/IncrementalMetaInformationExample_step3.tdms)
add_test(NAME dump_step4 COMMAND tdms_dump_structure ${CMAKE_SOURCE_DIR}/tdms_example_files/tdms-file-format-internal-structure/IncrementalMetaInformationExample_step4.tdms)
add_test(NAME dump_step5 COMMAND tdms_dump_structure ${CMAKE_SOURCE_DIR}/tdms_example_files/tdms-file-format-internal-structure/IncrementalMetaInformationExample_step5.tdms)
add_test(NAME dump_step6 COMMAND tdms_dump_structure ${CMAKE_SOURCE_DIR}/tdms_example_files/tdms-file-format-internal-structure/IncrementalMetaInformationExample_step6.tdms)

add_test(NAME dump_usage COMMAND tdms_dump_structure)
set_tests_properties(dump_usage
  PROPERTIES PASS_REGULAR_EXPRESSION "USAGE:.*"
  )