cmake_minimum_required(VERSION 3.5)

project(mount-gui VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTORCC_OPTIONS "--threshold;25;--compress-algo;best")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        config.h
        base.cpp
        base.h
        devmap.cpp
        devmap.h
        netmap.cpp
        netmap.h
        wsd_probe.cpp
        wsd_probe.h
        mount.cpp
        mount.h
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        systemd_dialog.cpp
        systemd_dialog.h
        systemd_dialog.ui
        resources.qrc
        common/qstr.h
        common/str.h
        common/path.h
        common/regex.h
        common/tiniline.h
        common/tpopen.cpp
        common/tpopen.h
        common/ucs.h
        common/vect_op.h
        common/fmt_op.h
        common/glob.h
        common/execute.h
        common/hires_timer.h
        common/systemd_escape.h
        common/human_readable.h
        common/optionparser.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(mount-gui
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET mount-gui APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(mount-gui SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(mount-gui
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(mount-gui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(mount-gui PRIVATE mtp)

if(DEFINED AFT_MTP_BEFORE_20230722)
  add_compile_definitions(AFT_MTP_BEFORE_20230722)
endif()

set_target_properties(mount-gui PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER kirillnow.no-ip.org.mount-gui
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS mount-gui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES org.kirillnow.mount-gui.policy
        DESTINATION /usr/share/polkit-1/actions)
install(FILES mount-gui.rules
        DESTINATION /etc/polkit-1/rules.d)
install(FILES mount-gui.desktop
        DESTINATION /usr/share/applications)
install(FILES icons/mount-gui.png
        DESTINATION /usr/share/icons/hicolor/16x16/apps)
install(FILES icons/mount-gui_32.png
        DESTINATION /usr/share/icons/hicolor/32x32/apps RENAME mount-gui.png)
install(FILES LICENSE.txt
        DESTINATION /usr/share/licenses/mount-gui RENAME LICENSE.GPL2)
install(FILES common/LICENSE.txt
        DESTINATION /usr/share/licenses/mount-gui RENAME LICENSE.MIT)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(mount-gui)
endif()
