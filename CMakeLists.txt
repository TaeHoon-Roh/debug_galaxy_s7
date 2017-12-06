cmake_minimum_required(VERSION 3.4.1)

add_library( # Sets the name of the library.
             native-lib

             SHARED

             src/main/cpp/native-lib.cpp src/main/cpp/usb_init.cpp src/main/cpp/fxload.cpp src/main/cpp/usbi_backend.cpp src/main/cpp/usb_set.cpp src/main/cpp/usb_open.cpp src/main/cpp/listdevs.cpp
             src/main/cpp/usb_exit.cpp src/main/cpp/usb_core.cpp)

find_library( log-lib log )

target_link_libraries( native-lib ${log-lib} )