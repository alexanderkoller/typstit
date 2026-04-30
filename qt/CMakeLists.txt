cmake_minimum_required(VERSION 3.16)

# OBJCXX (Objective-C++) is only available on Apple platforms.
if(APPLE)
    project(Typstit VERSION 0.1.0 LANGUAGES CXX OBJCXX)
else()
    project(Typstit VERSION 0.1.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Pdf PdfWidgets Concurrent)

qt_add_executable(typstit
    main.cpp
    mainwindow.cpp
    mainwindow.h
    previewwidget.cpp
    previewwidget.h
    compiler.cpp
    compiler.h
    highlighter.cpp
    highlighter.h
    preamble.h
    history.cpp
    history.h
    historywindow.cpp
    historywindow.h
    clipboard_mac.h
)

# Platform-specific clipboard implementation
if(APPLE)
    target_sources(typstit PRIVATE clipboard_mac.mm)
    target_link_libraries(typstit PRIVATE "-framework AppKit")
else()
    target_sources(typstit PRIVATE clipboard_stub.cpp)
endif()

qt_add_resources(typstit "app_resources"
    PREFIX "/"
)

target_link_libraries(typstit PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Concurrent
)

if(APPLE)
    # `cmake --build build --target deploy` produces a self-contained
    # Typstit.dmg with all Qt frameworks and plugins bundled inside the .app.
    find_program(MACDEPLOYQT macdeployqt
        HINTS "${Qt6_DIR}/../../../bin" "/opt/homebrew/bin")

    if(MACDEPLOYQT)
        add_custom_target(deploy
            COMMAND "${MACDEPLOYQT}"
                    "$<TARGET_BUNDLE_DIR:typstit>"
                    -dmg
            DEPENDS typstit
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Bundling Qt frameworks and creating Typstit.dmg..."
            VERBATIM
        )
    else()
        message(WARNING "macdeployqt not found; 'deploy' target unavailable")
    endif()
endif()

set_target_properties(typstit PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_IDENTIFIER com.typstit.qt
    MACOSX_BUNDLE_BUNDLE_NAME "Typstit"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE TRUE
)
