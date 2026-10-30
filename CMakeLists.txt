cmake_minimum_required(VERSION 3.16)
project(UrentQRGenerator VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
add_executable(${PROJECT_NAME}
    main.cpp QRGenerator.cpp QRGenerator.h
    third_party/qrcodegen/qrcodegen.cpp
    third_party/qrcodegen/qrcodegen.hpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::PrintSupport)
