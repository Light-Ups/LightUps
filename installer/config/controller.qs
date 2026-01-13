function Controller() {

}

Controller.prototype.IntroductionPageCallback = function() {
    var page = gui.pageById(QInstaller.Introduction); // Gebruik de ID uit de manual
// Schakel de Settings knop uit via de officiële GuiProxy methode
    gui.setSettingsButtonEnabled(false);

    if (page != null) {
        // Gebruik gui.findChild zoals getoond in de manual
        var packageManagerBtn = gui.findChild(page, "PackageManagerRadioButton");
        var updaterBtn = gui.findChild(page, "UpdaterRadioButton");
        var uninstallerBtn = gui.findChild(page, "UninstallerRadioButton");

        if (packageManagerBtn) packageManagerBtn.visible = false;
        if (updaterBtn) updaterBtn.visible = false;
        
        // Optioneel: Forceer de tekst om te zien of het script nu wél doorloopt
        // var messageLabel = gui.findChild(page, "MessageLabel");
        // if (messageLabel) {
            // messageLabel.setText("Kies 'Next' om de applicatie volledig te verwijderen.");
        // }
    }
}

Controller.prototype.FinishedPageCallback = function() {
    var widget = gui.currentPageWidget(); // Verkrijg de huidige pagina
    if (widget && widget.RunItCheckBox) {
        widget.RunItCheckBox.checked = true; // Vink de 'Run application' checkbox aan
    }
}