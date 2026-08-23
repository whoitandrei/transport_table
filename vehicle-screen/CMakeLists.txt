cmake_minimum_required(VERSION 3.21)
project(simpleapp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Protobuf
    ProtobufWellKnownTypes
    ProtobufWellKnownTypesPrivate
    Grpc
)

set(PROTO_DIR ${CMAKE_SOURCE_DIR}/proto)

qt_add_protobuf(board_proto
    PROTO_FILES
        ${PROTO_DIR}/common.proto
        ${PROTO_DIR}/weather.proto
        ${PROTO_DIR}/route.proto
        ${PROTO_DIR}/content.proto
        ${PROTO_DIR}/board.proto
    PROTO_INCLUDES
        ${PROTO_DIR}
)

qt_add_grpc(board_grpc CLIENT
    PROTO_FILES
        ${PROTO_DIR}/board.proto
    PROTO_INCLUDES
        ${PROTO_DIR}
)

# generated code pulls in well-known-types private serializers
target_link_libraries(board_proto PUBLIC
    Qt6::ProtobufWellKnownTypes
    Qt6::ProtobufWellKnownTypesPrivate
)

target_link_libraries(board_grpc PRIVATE board_proto)

qt_add_executable(simpleapp
    src/main.cpp
    src/backend.h
    src/backend.cpp
    src/time_manager.h
    src/time_manager.cpp
    src/board_client.h
    src/board_client.cpp
    qml/qml.qrc
)

target_link_libraries(simpleapp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Protobuf
    Qt6::Grpc
    board_proto
    board_grpc
)

if(APPLE)
    set_target_properties(simpleapp PROPERTIES MACOSX_BUNDLE TRUE)
endif()
