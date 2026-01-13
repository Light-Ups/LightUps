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

add_executable(LightUpsGui WIN32
  main.cpp
  systemtrayapp.h systemtrayapp.cpp
  app_resources.qrc
  upsiconmanager.h upsiconmanager.cpp
  upsstatuswindow.h upsstatuswindow.cpp
  upsstatuswindow.ui
)

qt_add_translations(LightUpsGui TS_FILES
  LightUpsGui_pt_BR.ts
  LightUpsGui_nl_NL.ts)

target_link_libraries(LightUpsGui PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt6::Widgets Qt::SerialPort Qt::Network Qt6::Svg Qt6::Xml ups_headers LightUpsApi)

# Koppel de benodigde Windows API bibliotheken
# Dit blok zorgt ervoor dat de linker de SetSuspendState en de Shutdown/Token functies vindt.
if (WIN32)
    target_link_libraries(LightUpsGui
        PRIVATE
            User32
            Advapi32
            PowrProf  # <-- DEZE LIB LOST DE FOUT OP!
    )
endif()
