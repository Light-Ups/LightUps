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

qt_add_plugin(nhs_driver
        nhs_driver.h
        nhs_driver.cpp
)

target_sources(nhs_driver
  PRIVATE
    nhs_driver.json
)

target_link_libraries(nhs_driver PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt6::SerialPort ups_headers LightUpsApi)

target_compile_definitions(nhs_driver PRIVATE NHS_DRIVER_LIBRARY)

add_custom_command(
    TARGET nhs_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:nhs_driver>"
    "${CMAKE_CURRENT_BINARY_DIR}/../"
    COMMENT "Kopieert nhs_driver.dll naar de common/plugins map"
)
