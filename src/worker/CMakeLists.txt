FILE(GLOB WORKER_SRC *.cpp)
FILE(GLOB UTIL_HEADERS ${CMAKE_BINARY_DIR}/src/util/*.h)
FILE(GLOB UTIL_SOURCE ${CMAKE_BINARY_DIR}/src/util/*.cpp)
include_directories(${CMAKE_BINARY_DIR}/proto)
include_directories(${CMAKE_BINARY_DIR}/src/util)
add_executable(worker ${WORKER_SRC} ${UTIL_SOURCE} ${UTIL_HEADERS})
target_link_libraries(worker hw_grpc_proto ${_REFLECTION} ${ZOOKEEPER_LIBRARIES} ${CONSERVATOR_LIBRARIES} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${_GLOG} ${CONSERVATOR_LIBRARY} ${Boost_LIBRARIES}  ${SSL_LIBRARIES} ${CASABLANCA_LIBRARY} ${AZURE_STORAGE_LIBRARY})