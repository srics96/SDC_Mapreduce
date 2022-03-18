cmake_minimum_required(VERSION 3.2)
project(mapreduce)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules/")

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(Zookeeper REQUIRED)
find_package(Check REQUIRED)
find_package(Conservator REQUIRED)


find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

find_package(glog 0.5.0 REQUIRED)

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
set(_GLOG glog::glog)

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP gRPC::grpc++)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

message(STATUS ${CMAKE_SOURCE_DIR})
set(PROTO_PATH "${CMAKE_SOURCE_DIR}/protos")
set(MAPREDUCE_PROTO "${CMAKE_SOURCE_DIR}/protos/central.proto")
set(GENERATED_PROTOBUF_PATH "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

set(MAPREDUCE_PB_CPP_FILE "${GENERATED_PROTOBUF_PATH}/central.pb.cc")
set(MAPREDUCE_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/central.pb.h")
set(MAPREDUCE_GRPC_PB_CPP_FILE "${GENERATED_PROTOBUF_PATH}/central.grpc.pb.cc")
set(MAPREDUCE_GRPC_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/central.grpc.pb.h")
add_custom_command(
    OUTPUT "${MAPREDUCE_PB_H_FILE}"
           "${MAPREDUCE_PB_CPP_FILE}"
           "${MAPREDUCE_GRPC_PB_H_FILE}"
           "${MAPREDUCE_GRPC_PB_CPP_FILE}"
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS "--proto_path=${PROTO_PATH}"
         "--cpp_out=${GENERATED_PROTOBUF_PATH}"
         "${MAPREDUCE_PROTO}"
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS "--proto_path=${PROTO_PATH}"
         "--grpc_out=${GENERATED_PROTOBUF_PATH}"
         "--plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin"
         "${MAPREDUCE_PROTO}"
    )

add_library(hw_grpc_proto ${MAPREDUCE_PB_H_FILE} ${MAPREDUCE_PB_CPP_FILE} ${MAPREDUCE_GRPC_PB_H_FILE} ${MAPREDUCE_GRPC_PB_CPP_FILE})
target_link_libraries(hw_grpc_proto ${_REFLECTION} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF})


set(GENERATED_PROTOBUF_FILES ${MAPREDUCE_PB_H_FILE} ${MAPREDUCE_PB_CPP_FILE}
    ${MAPREDUCE_GRPC_PB_H_FILE} ${MAPREDUCE_GRPC_PB_CPP_FILE})

include_directories(${GENERATED_PROTOBUF_PATH})
include_directories(${CHECK_INCLUDE_DIRS})
include_directories(${ZOOKEEPER_INCLUDE_DIR}/zookeeper)
include_directories(${CONSERVATOR_INCLUDE_DIR}/conservator)

add_library(conservator_lib SHARED IMPORTED)
set_property(TARGET conservator_lib PROPERTY IMPORTED_LOCATION "${CONSERVATOR_INCLUDE_DIR}/conservator/build/libconservator-framework.so")


link_libraries(grpc++_unsecure grpc gpr ${PROTOBUF_LIBRARY})

add_executable(master src/master/master.cc )
target_link_libraries(master hw_grpc_proto ${_REFLECTION}  ${ZOOKEEPER_LIBRARIES} ${CONSERVATOR_LIBRARIES} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${_GLOG} ${CONSERVATOR_LIBRARY})
add_executable(worker src/worker/worker.cc )
target_link_libraries(worker hw_grpc_proto ${_REFLECTION} ${ZOOKEEPER_LIBRARIES} ${CONSERVATOR_LIBRARIES} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${_GLOG} ${CONSERVATOR_LIBRARY})

