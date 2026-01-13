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

add_library(ups_headers INTERFACE)

# Vertel CMake waar de bestanden staan
target_include_directories(ups_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Optioneel: voeg ze toe aan de sources zodat ze in de zijbalk verschijnen
target_sources(ups_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc_constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ups_report.h
)
