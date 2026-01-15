# LightUps
This project provides a lightweight Windows service and client application designed for real-time UPS monitoring and automated intervention. Built with Qt 6.9, the software emphasizes a user-friendly experience with simplified settings and a flexible plugin system for supporting various hardware drivers.

### 🔌 Hardware Support & Drivers
LightUps uses a modular architecture where hardware communication is handled through dedicated plugin drivers.

* **NHS Mini Senoidal:** Currently, this is the primary supported hardware driver.
* **Dummy Driver:** A built-in dummy driver is included for testing purposes, allowing you to explore the software's functionality without requiring physical UPS hardware.

Future updates will expand support for additional UPS models through our plugin system.
