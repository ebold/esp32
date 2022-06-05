# Hello World Example

Starts a FreeRTOS task to print "Hello World" on the SSD1306 monochrome display using the LVGL (v8.2+) ESP32 driver.

This example is derived from an existing "Hello World" example from the ESP-IDF and a demo from the "lv_port_esp32" project.

## 1. How to run this example

Follow detailed instructions to set up development environment and run this example. Tested on a host system with **Linux Mint 19.3**.

### 1.1. General

```
$ sudo apt update && sudo apt upgrade -y
```

### 1.2. Install [Pyenv](https://realpython.com/intro-to-pyenv/#installing-pyenv)

```
$ sudo apt-get install -y make build-essential libssl-dev zlib1g-dev libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncurses5-dev libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev python-openssl
$ curl https://pyenv.run | bash
```

Load pyenv automatically by adding the following commands to your **~/.bashrc**:

```
export PATH="$HOME/.pyenv/bin:$PATH"
eval "$(pyenv init --path)"
eval "$(pyenv virtualenv-init -)"
```

### 1.3. Install ESP-IDF, [getting started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

```
$ git clone -b v4.4.1 --recursive https://github.com/espressif/esp-idf.git esp/esp-idf-v4.4.1
$ cd esp/esp-idf-v4.4.1/
$ ./install.sh all
$ . ./export.sh         # add an alias 'get_idf' to your .bashrc
```

### 1.4. Install the LVGL library ported to ESP32, [lv_port_esp32](https://github.com/lvgl/lv_port_esp32)

Ensure that you're in the **~/esp/esp-idf-v4.4.1** folder. Add LVGL as submodule.

```
$ git submodule add https://github.com/lvgl/lvgl.git components/lvgl
$ git submodule add https://github.com/lvgl/lvgl_esp32_drivers.git components/lvgl_esp32_drivers
```

### 1.5. Install Eclipse, [Eclipse IDE for C/C++ Developers installation on Ubuntu 20.04](https://linuxconfig.org/eclipse-ide-for-c-c-developers-installation-on-ubuntu-20-04)

```
$ sudo tar xvf ~/Downloads/eclipse-cpp-2021-09-R-linux-gtk-x86_64.tar.gz -C /opt/
$ sudo ln -s /opt/eclipse/eclipse /usr/local/bin/
```

Create desktop launcher (Ubuntu) or menu launcher (Linux Mint)

### 1.6. Install the ESP-IDF plug-in for Eclipse, [idf-eclipse-plugin](https://github.com/espressif/idf-eclipse-plugin)

Just follow the provided instructions.
Now you're ready to run this example from Eclipse.

### 1.7. Run this example

Import this example as a new project.
Build and run this demo. Follow the configure and build instructions from the ESP-IDF [Get started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) guide.

## 2. Example folder contents

The project **hello_world** contains one source file in C language [hello_world_main.c](main/hello_world_main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── example_test.py            Python script used for automated example testing
├── main
│   ├── CMakeLists.txt
│   ├── component.mk           Component make file
│   └── hello_world_main.c
├── Makefile                   Makefile used by legacy GNU Make
└── README.md                  This is the file you are currently reading
```

The content of **hello_world_main.c** is derived mostly from a demo in [lv_port_esp32](https://github.com/lvgl/lv_port_esp32) and an another [demo](https://chowdera.com/2022/04/202204132319202760.html) with the LVGL v8.2.

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## 3. Troubleshooting

### 3.1. Do not add **lv_conf.h** in your source manually.

* LVGL supports ESP-IDF's menuconfig, so configure it with 'idf.py menuconfig' and choose 'Component config' -> 'LVGL configuration'. Refer to the ESP-IDF [Get started](https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/stable/get-started/index.html) guide.

### 3.2. Run-time error with I2C Manager: E (30) lvgl_i2c: Invalid port or not configured for I2C Manager: 0

* If above run-time error occurs, then the chosen I2C port used by the display must be enabled in **sdkconfig**.
* In 'menuconfig': 'Component config' -> 'I2C Port Settings' -> 'Enable I2C port 0' -> set SDA, SCL port pins (ie., SDA=21, SCL=22)

### 3.3. LV_HOR_RES_MAX, LV_VER_RES_MAX are not set in sdkconfig since LVGL v8.x

* Define the maximum display resolution in '**lvgl_helpers.h**'. Refer this [post](https://forum.lvgl.io/t/lv-hor-res-max-and-lv-ver-res-max/5817/2) for some hints.
 
### 3.4. Program upload failure

* Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
* The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## 4. (Optional) Run a demo provided in LVGL port to ESP32, [lv_port_esp32](https://github.com/lvgl/lv_port_esp32)

Download 'lv_port_esp32' example from Git repo.
Replace default ILI9341 with SSD1306 (changes sdkconfig).
Compilation error can be fixed by editing ssd1306.c file.

### 4.1. Install binary image to ESP32

Assume the ESP32 board is connected as '/dev/ttyUSB0'.
To flash the binary image, invoke a following command:

```
$ idf.py -p /dev/ttyUSB0 flash
```

### 4.2. Monitor the application

The 'IDF Monitor' application is used to see the debug output message:

```
$ idf.py -p /dev/ttyUSB0 monitor
```

Both flashing and monitoring commands can be combined:
$ idf.py -p /dev/ttyUSB0 flash monitor.