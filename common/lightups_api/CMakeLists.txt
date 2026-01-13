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

add_library(LightUpsApi SHARED
  lightups_api.cpp
  lightups_api.h
  i_ups_driver.h
  registry_watcher.h registry_watcher.cpp
  i_ups_driver.cpp
)

target_link_libraries(LightUpsApi PRIVATE Qt${QT_VERSION_MAJOR}::Core ups_headers)

target_compile_definitions(LightUpsApi PRIVATE
  UPS_API_LIBRARY_LIBRARY
  COMMON_PLUGINS_DIR="${COMMON_PLUGINS_PATH}"
)

target_include_directories(LightUpsApi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
