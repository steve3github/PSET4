cmake_minimum_required(VERSION 3.16)
project(PSET4 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/client
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)


# Qt
set(CMAKE_PREFIX_PATH "C:/QtMSVC/6.9.3/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Force CMake to look in vcpkg's installed directory
set(CMAKE_PREFIX_PATH 
    "${CMAKE_PREFIX_PATH}"
    "C:/Users/user/vcpkg/installed/x64-windows"
)

# Find packages without CONFIG mode to avoid wrapper issues
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(Tesseract REQUIRED)
find_package(Leptonica REQUIRED)

# Proto files
set(PROTO_SRCS proto/ocr.pb.cc)
set(PROTO_HDRS proto/ocr.pb.h)
set(GRPC_SRCS proto/ocr.grpc.pb.cc)
set(GRPC_HDRS proto/ocr.grpc.pb.h)

# Server
add_executable(server
    server/main.cpp
    server/ocr_server.cpp
    server/ocr_server.h
    server/worker_pool.cpp
    server/worker_pool.h
    proto/ocr.pb.cc
    proto/ocr.grpc.pb.cc
)
target_link_libraries(server
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    libtesseract
    leptonica
)

# Client
add_executable(client
    client/main.cpp
    client/mainwindow.cpp
    client/mainwindow.h
    client/grpc_client.cpp
    client/grpc_client.h
    proto/ocr.pb.cc
    proto/ocr.grpc.pb.cc
)
target_link_libraries(client
    PRIVATE
    Qt6::Widgets
    gRPC::grpc++
    protobuf::libprotobuf
    libtesseract
    leptonica
)