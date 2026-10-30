cmake_minimum_required(VERSION 3.21)

project(UrentQRGenerator VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_executable(UrentQRGenerator
    main.cpp
    QRGenerator.cpp
    QRGenerator.h
    third_party/qrcodegen/qrcodegen.cpp
    third_party/qrcodegen/qrcodegen.hpp
)

target_include_directories(UrentQRGenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen
)

target_link_libraries(UrentQRGenerator PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
)

if(MSVC)
    target_compile_options(UrentQRGenerator PRIVATE /W4 /utf-8)
else()
    target_compile_options(UrentQRGenerator PRIVATE -Wall -Wextra -Wpedantic)
endif()
