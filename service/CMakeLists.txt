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

add_executable(LightUpsService
    main.cpp
    ups_ipc_server.h ups_ipc_server.cpp
    ups_monitor_service.h ups_monitor_service.cpp
    windows_service.h
    windows_service.cpp
    nobreak_messages.mc
)

target_link_libraries(LightUpsService PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt::SerialPort
    Qt6::Network
    ups_headers
    LightUpsApi
)
