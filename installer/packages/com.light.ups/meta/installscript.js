function Component() {
    // Forceer admin rechten voor globale HKLM registratie
    installer.setValue("RequiresAdminRights", "true");
    installer.setValue("AllUsers", "true");
    // Deze variabelen vertellen de engine HOE de uninstaller in Windows geregistreerd moet worden
    installer.setValue("NoModify", "true");
    installer.setValue("NoRepair", "true");
    
    // Optioneel: Dit voorkomt dat de 'Modify' knop in de MaintenanceTool zelf actief is
    installer.setValue("ModifyDefault", "false");
}

Component.prototype.createOperations = function() {
    // Bestanden uitpakken (data map)
    component.createOperations();

    if (systemInfo.productType === "windows") {
        var targetDir = installer.value("TargetDir").replace(/\//g, "\\");
        var exePath = targetDir + "\\bin\\LightUpsService.exe";
        var exeGuiPath = targetDir + "\\bin\\LightUpsGui.exe";

        // 1. Haal de paden op
        var userStartMenu = installer.value("UserStartMenuProgramsPath");
        var allUsersStartMenu = installer.value("AllUsersStartMenuProgramsPath");
        var currentStartMenuDir = installer.value("StartMenuDir");

        // 2. De 'substitute' logica: 
        // Vervang het persoonlijke pad-gedeelte door het publieke pad-gedeelte
        var finalStartMenuPath = currentStartMenuDir.replace(userStartMenu, allUsersStartMenu);

        // 3. Map aanmaken (Mkdir) op de nieuwe locatie
        component.addOperation("Mkdir", finalStartMenuPath);

        // 4. Snelkoppeling aanmaken
        component.addOperation("CreateShortcut", 
            exeGuiPath, 
            finalStartMenuPath + "\\Light UPS Monitor.lnk", 
            "workingDirectory=" + targetDir + "\\bin", 
            "description=Monitor de status van de UPS",
            "iconPath=" + exeGuiPath, "iconId=0"
        );

        // --- SERVICE INSTALLATIE ---
        
        component.addElevatedOperation("Execute", 
            "sc", "create", "LightUPSService", "binPath=", "\"" + exePath + "\"", "start=", "auto",
            "UNDOEXECUTE", 
            "sc", "delete", "LightUPSService"
        );

        component.addElevatedOperation("Execute", 
            "sc", "start", "LightUPSService",
            "UNDOEXECUTE", 
            "sc", "stop", "LightUPSService"
        );

// --- EVENT LOG REGISTRATIE ---
        var registryLogPath = "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\LightUps";
        
        // 1. Optioneel: Verwijder de gehele sleutel (map) pas als laatste stap bij de-installatie
        // Dit zorgt voor een schone registry zonder lege mappen.
        component.addElevatedOperation("Execute", "cmd", "/C", "exit", "0", 
            "UNDOEXECUTE", "reg", "delete", registryLogPath, "/f"
        );

        // 2. Voeg EventMessageFile toe - UNDO verwijdert alleen deze specifieke waarde
        component.addElevatedOperation("Execute", 
            "reg", "add", registryLogPath, "/v", "EventMessageFile", "/t", "REG_SZ", "/d", "%SystemRoot%\\System32\\EventCreate.exe", "/f",
            "UNDOEXECUTE", 
            "reg", "delete", registryLogPath, "/v", "EventMessageFile", "/f"
        );
        
        // 3. Voeg TypesSupported toe - UNDO verwijdert alleen deze specifieke waarde
        component.addElevatedOperation("Execute", 
            "reg", "add", registryLogPath, "/v", "TypesSupported", "/t", "REG_DWORD", "/d", "7", "/f",
            "UNDOEXECUTE", 
            "reg", "delete", registryLogPath, "/v", "TypesSupported", "/f"
        );

        // --- AUTOSTART REGISTRY (HKLM voor alle gebruikers) ---
        
        component.addElevatedOperation("Execute", 
            "reg", "add", "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
            "/v", "LightUPSMonitor", "/t", "REG_SZ", "/d", "\"" + exeGuiPath + "\"", "/f",
            "UNDOEXECUTE", 
            "reg", "delete", "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
            "/v", "LightUPSMonitor", "/f"
        );

        // Start de GUI direct op na de installatie

        // Start de GUI direct op na de installatie ZONDER te wachten
        component.addOperation("Execute", 
            "cmd", "/C", "start", "", exeGuiPath,
            "UNDOEXECUTE", 
            "cmd", "/C", "taskkill /F /IM LightUpsGui.exe /T || exit 0"
        );
    }
}