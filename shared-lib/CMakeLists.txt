add_definitions("-DBUILDING_BTRF")

file(GLOB_RECURSE IMPL_SRC_LIST "${SHARED_LIB_PATH}/impl/*.cpp" "${SHARED_LIB_PATH}/impl/*.h")
file(GLOB_RECURSE COMMON_SRC_LIST "${SHARED_LIB_PATH}/common/*.cpp" "${SHARED_LIB_PATH}/common/*.h")
file(GLOB_RECURSE INTERFACES_SRC_LIST "${SHARED_LIB_PATH}/interfaces/*.cpp" "${SHARED_LIB_PATH}/interfaces/*.h")

include_directories("${SHARED_LIB_PATH}/impl")

add_library(${SHARED_LIB_NAME} SHARED ${IMPL_SRC_LIST} ${COMMON_SRC_LIST} ${INTERFACES_SRC_LIST})
