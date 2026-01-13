; // nobreak_messages.mc

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0:FACILITY_SYSTEM
               Runtime=0x2:FACILITY_RUNTIME
               User=0x3:FACILITY_USER
              )

LanguageNames=(Dutch=0x413:MSG00413)

; // Berichten definities

MessageId=100
Severity=Informational
Facility=Runtime
SymbolicName=MSG_SERVICE_INFO
Language=Dutch
Service Informatie: %1
.

MessageId=200
Severity=Informational
Facility=Runtime
SymbolicName=MSG_POWER_RESTORED
Language=Dutch
Netspanning status: %1
.

MessageId=300
Severity=Warning
Facility=Runtime
SymbolicName=MSG_ON_BATTERY
Language=Dutch
Waarschuwing: %1
.

MessageId=400
Severity=Error
Facility=Runtime
SymbolicName=MSG_BATT_CRITICAL
Language=Dutch
KRITIEK: %1
.
