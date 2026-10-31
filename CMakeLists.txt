cmake_minimum_required(VERSION 3.20)
project(rog-power-manager VERSION 0.4.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)

set(SOURCES
    src/main.cpp
    src/core/Config.hpp
    src/core/Config.cpp
    src/core/Translations.hpp
    src/core/Hardware.hpp
    src/core/Hardware.cpp
    src/core/IdleManager.hpp
    src/core/IdleManager.cpp
    src/core/LidManager.hpp
    src/core/LidManager.cpp
    src/core/Controller.hpp
    src/core/Controller.cpp
    src/dbus/DBusServices.hpp
    src/dbus/DBusServices.cpp
    src/gui/TrayIcon.hpp
    src/gui/TrayIcon.cpp
    src/gui/SettingsDialog.hpp
    src/gui/SettingsDialog.cpp
    src/gui/AboutDialog.hpp
    src/gui/AboutDialog.cpp
)

add_executable(rog-power-manager ${SOURCES})

target_link_libraries(rog-power-manager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
)

target_compile_definitions(rog-power-manager PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

target_include_directories(rog-power-manager PRIVATE
    src
    src/core
    src/dbus
    src/gui
)

include(GNUInstallDirs)
install(TARGETS rog-power-manager
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
