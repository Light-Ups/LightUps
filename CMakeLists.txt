 # LightUps: A lightweight Qt-based UPS monitoring service and client for Windows.
 # Copyright (C) 2026 Andreas Hoogendoorn (@andhoo)
 #
 # This program is free software: you can redistribute it and/or modify
 # it under the terms of the GNU Affero General Public License as published
 # by the Free Software Foundation, either version 3 of the License, or
 # (at your option) any later version.
 #
 # This program is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 # GNU Affero General Public License for more details.
 #
 # You should have received a copy of the GNU Affero General Public License
 # along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)
project(LightUps VERSION 1.0.0 LANGUAGES CXX)
string(TIMESTAMP CURRENT_YEAR "%Y")

add_definitions(
    -DAPP_VERSION="${PROJECT_VERSION}"
    -DCOPYRIGHT_YEAR="${CURRENT_YEAR}"
)

string(TIMESTAMP CURRENT_DATE "%Y-%m-%d")

# Genereer de config.xml voor QTIFW
set(ApplicationsDir "@ApplicationsDir@")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/config/config.xml.in
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/config/config.xml
    @ONLY
)

# # En voor de package.xml (als je daar ook de versie wilt)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/packages/com.light.ups/meta/package.xml.in
    ${CMAKE_CURRENT_SOURCE_DIR}/installer/packages/com.light.ups/meta/package.xml
    @ONLY
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-2")
# add_compile_options(-fno-omit-frame-pointer)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(COMMON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/common/include")

# ----------------------------------------------------------------------------------
# QT MODULEN EN SUBPROJECTEN
# ----------------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core SerialPort Widgets Network Svg Xml LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core SerialPort Widgets Network Svg Xml LinguistTools)

# set(TS_LANGUAGES "en" "nl")
qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES nl en pt)

add_subdirectory(common/include)
add_subdirectory(common/lightups_api)

# Plugins/Drivers
set(PLUGIN_DIRECTORIES
    common/plugins/nhs_driver
    common/plugins/template_driver
)

# Lus door de lijst en voeg elke driver toe
message(STATUS "Adding plugins: ${PLUGIN_DIRECTORIES}")
foreach(PLUGIN_DIR ${PLUGIN_DIRECTORIES})
    add_subdirectory(${PLUGIN_DIR})
endforeach()

# Executables
add_subdirectory(gui)
add_subdirectory(service)

# 1. Definieer het pad naar de 'data' map
set(INSTALLER_DATA_DIR "${CMAKE_SOURCE_DIR}/installer/packages/com.light.ups/data")

# 2. Installeer Executables & API Library
install(TARGETS LightUpsGui LightUpsService LightUpsApi
    RUNTIME DESTINATION "${INSTALLER_DATA_DIR}/bin"
)

# 3. SCHONE VERTALINGEN (Zoekt pas TIJDENS installatie)
install(CODE "
    file(GLOB QM_FILES \"${CMAKE_BINARY_DIR}/gui/*.qm\")
    if(QM_FILES)
        file(INSTALL DESTINATION \"${INSTALLER_DATA_DIR}/bin/i18n\"
             TYPE FILE
             FILES \${QM_FILES})
        message(STATUS \"Gevonden vertalingen geïnstalleerd in i18n/\")
    endif()
")

# 4. DRIVERS (Zoekt pas TIJDENS installatie)
install(CODE "
    file(GLOB DRIVER_DLLS \"${CMAKE_BINARY_DIR}/common/plugins/*.dll\")
    if(DRIVER_DLLS)
        file(INSTALL DESTINATION \"${INSTALLER_DATA_DIR}/bin/common/plugins\"
             TYPE FILE
             FILES \${DRIVER_DLLS})
        message(STATUS \"Gevonden drivers geïnstalleerd in drivers/\")
    endif()
")

# 5. Assets (SVG)
install(FILES "gui/ups_status.svg" DESTINATION "${INSTALLER_DATA_DIR}/bin/assets")

# 6. Windows DLL's (Windeployqt) - Dit is essentieel voor Qt apps!
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --compiler-runtime \"${INSTALLER_DATA_DIR}/bin/LightUpsGui.exe\" \"${INSTALLER_DATA_DIR}/bin/LightUpsService.exe\"
        )
    ")
endif()
