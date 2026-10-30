cmake_minimum_required(VERSION 3.16)
project(UrentQRGenerator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

# Автоматическая загрузка и сборка libqrencode из GitHub
include(FetchContent)
FetchContent_Declare(
    qrencode
    GIT_REPOSITORY https://github.com/fukuchi/libqrencode.git
    GIT_TAG        v4.1.1
)
FetchContent_MakeAvailable(qrencode)

add_executable(${PROJECT_NAME}
    main.cpp
    QRGenerator.cpp
    QRGenerator.h
)

# Подключаем Qt и собранную библиотеку qrencode
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Widgets 
    Qt6::PrintSupport 
    qrencode
)
