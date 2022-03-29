# Repository for ESP32 project

# Table Of Contents
* [ Installing Prerequisites ](#Prerequisites) <br>
* [ Hardware ](#Hardware) <br>
* [ Task tracking ](#Task) <br>

<a name="Prerequisites"></a>
# Installing Prerequisites
* **Java 8 and above** : Download and install Java SE from <a href= "https://www.oracle.com/technetwork/java/javase/downloads/index.html">here</a>
* **Python 3.5 and above** : Download and install Python from <a href="https://www.python.org/downloads/">here</a>
* **Eclipse 2018-12 CDT and above** : Download and install Eclipse CDT package from <a href= "https://www.eclipse.org/downloads/packages/release/2020-03/r/eclipse-ide-cc-developers-includes-incubating-components">here </a>
*  **Git** : Get the latest git from <a href ="https://git-scm.com/downloads">here</a>
*  **ESP-IDF 4.0 and above** : Clone the ESP-IDF repo from <a href ="https://github.com/espressif/esp-idf/releases">here</a>
*  **ESP Plugin IDF** : Download or install from Eclipse <a href = "https://marketplace.eclipse.org/content/esp-idf-eclipse-plugin"> here </a>
*  **ESP Plugin OpenOCD** : Download or install from Eclipse <a href = "https://marketplace.eclipse.org/content/esp32-cc-development-tools"> here </a>

**Note:** Make sure Java, Python and Git are available on the system environment PATH.

<a name="Hardware"></a>
# Hardware

*  **ESP32-S2 Development board** : Reference link from <a href = "https://www.espressif.com/en/products/devkits/esp32-devkitc/overview"> here </a>

<a name="Task"></a>
# Task tracking

- [x] Setup environment
- [ ] APP Scheduler 
  + - [x] Get/Set configuration scheduler 
  + - [x] Save configuration to NVS
  + - [ ] Get/Set current scheduler base on current time
- [x] WIFI Access Point 
  + - [x] Save data to NVS
- [x] TCP communication
  + - [x] Establish TCP Server with Port 39203 to communication with APP
  + - [x] Send/Rev data from client (App todo)
  + - [ ] Implement data frame between Server and Clients
