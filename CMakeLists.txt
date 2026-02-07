cmake_minimum_required(VERSION 3.20)
project(OpenLock
    VERSION 0.1.0
    DESCRIPTION "Open-source Linux lockdown exam browser"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Build options
option(OPENLOCK_BUILD_TESTS "Build unit tests" ON)
option(OPENLOCK_ENABLE_WAYLAND "Enable Wayland/Cage kiosk support" ON)
option(OPENLOCK_ENABLE_VM_DETECTION "Enable VM detection (can be disabled for dev)" ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    WebEngineCore
    Network
)

# Find system libraries
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(X11 IMPORTED_TARGET x11)
pkg_check_modules(XRANDR IMPORTED_TARGET xrandr)
pkg_check_modules(XCB IMPORTED_TARGET xcb xcb-keysyms xcb-xfixes xcb-randr)
pkg_check_modules(XKBCOMMON IMPORTED_TARGET xkbcommon)

# Core library (shared between main app and tests)
add_library(openlock_core STATIC
    # Core
    src/core/LockdownEngine.cpp
    src/core/Config.cpp

    # Kiosk
    src/kiosk/PlatformKiosk.cpp
    src/kiosk/KioskShell.cpp
    src/kiosk/X11Kiosk.cpp
    src/kiosk/WaylandKiosk.cpp

    # Guard
    src/guard/ProcessGuard.cpp
    src/guard/ProcessBlocklist.cpp
    src/guard/CGroupIsolator.cpp

    # Input
    src/input/InputLockdown.cpp
    src/input/ClipboardGuard.cpp
    src/input/ShortcutBlocker.cpp
    src/input/PrintBlocker.cpp

    # Integrity
    src/integrity/SystemIntegrity.cpp
    src/integrity/VMDetector.cpp
    src/integrity/DebugDetector.cpp
    src/integrity/SelfVerifier.cpp

    # Browser
    src/browser/SecureBrowser.cpp
    src/browser/NavigationFilter.cpp
    src/browser/DownloadBlocker.cpp
    src/browser/DevToolsBlocker.cpp

    # Protocol
    src/protocol/SEBProtocol.cpp
    src/protocol/SEBConfigParser.cpp
    src/protocol/BrowserExamKey.cpp
    src/protocol/ConfigKeyGenerator.cpp
    src/protocol/SEBRequestInterceptor.cpp

    # LMS
    src/lms/MoodleAdapter.cpp
    src/lms/CanvasAdapter.cpp
    src/lms/BlackboardAdapter.cpp
)

target_include_directories(openlock_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(openlock_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(X11_FOUND)
    target_link_libraries(openlock_core PUBLIC PkgConfig::X11)
    target_compile_definitions(openlock_core PUBLIC OPENLOCK_HAS_X11)
endif()

if(XRANDR_FOUND)
    target_link_libraries(openlock_core PUBLIC PkgConfig::XRANDR)
endif()

if(XCB_FOUND)
    target_link_libraries(openlock_core PUBLIC PkgConfig::XCB)
    target_compile_definitions(openlock_core PUBLIC OPENLOCK_HAS_XCB)
endif()

if(XKBCOMMON_FOUND)
    target_link_libraries(openlock_core PUBLIC PkgConfig::XKBCOMMON)
endif()

if(OPENLOCK_ENABLE_VM_DETECTION)
    target_compile_definitions(openlock_core PUBLIC OPENLOCK_VM_DETECTION)
endif()

if(OPENLOCK_ENABLE_WAYLAND)
    target_compile_definitions(openlock_core PUBLIC OPENLOCK_WAYLAND)
endif()

# Main executable
add_executable(openlock src/main.cpp)
target_link_libraries(openlock PRIVATE openlock_core)

# Copy data files to build directory for development runs
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/share/openlock)
configure_file(config/blocklist.json ${CMAKE_BINARY_DIR}/share/openlock/blocklist.json COPYONLY)
configure_file(config/default.openlock ${CMAKE_BINARY_DIR}/share/openlock/default.openlock COPYONLY)

# Install
install(TARGETS openlock RUNTIME DESTINATION bin)
install(FILES config/default.openlock DESTINATION share/openlock)
install(FILES config/blocklist.json DESTINATION share/openlock)

# Tests
if(OPENLOCK_BUILD_TESTS)
    enable_testing()
    find_package(GTest)

    if(GTest_FOUND)
        function(openlock_add_test TEST_NAME TEST_SOURCE)
            add_executable(${TEST_NAME} ${TEST_SOURCE})
            target_link_libraries(${TEST_NAME} PRIVATE openlock_core GTest::gtest GTest::gtest_main)
            add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        endfunction()

        openlock_add_test(test_process_guard tests/unit/test_process_guard.cpp)
        openlock_add_test(test_vm_detector tests/unit/test_vm_detector.cpp)
        openlock_add_test(test_seb_config tests/unit/test_seb_config.cpp)
        openlock_add_test(test_navigation_filter tests/unit/test_navigation_filter.cpp)
    else()
        message(WARNING "GTest not found, tests will not be built")
    endif()
endif()

# CPack for packaging
set(CPACK_PACKAGE_NAME "openlock")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-source Linux lockdown exam browser")
set(CPACK_PACKAGE_CONTACT "OpenLock Contributors")
set(CPACK_PACKAGE_LICENSE "MPL-2.0")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-webengine (>= 6.2)")
include(CPack)
