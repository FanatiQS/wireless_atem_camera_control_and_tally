set(SDKCONFIG ${CMAKE_BINARY_DIR}/sdkconfig)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR})

message("")
message("----------------")
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("SDKCONFIG: ${SDKCONFIG}")
message("EXTRA_COMPONENT_DIRS: ${EXTRA_COMPONENT_DIRS}")
message("----------------")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
