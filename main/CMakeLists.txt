# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Define the source files
set(SOURCES 
    "main.c" 
    "wifi.c" 
    "mqtt.c" 
    "led.c" 
    "state_handler.c"
    "mp3.c"
    "spiffs.c"
    "ota.c"
    "certs/amazon_root_ca1.c"
    "certs/coop_snooper_cert.c"
    "certs/coop_snooper_private_key.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Add the source files to the build
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDES}
)

