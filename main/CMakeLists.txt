# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Define the source files
set(SOURCES 
    "main.c" 
    "wifi.c" 
    "mqtt.c" 
    "led.c" 
    "state_handler.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Specify the files to be embedded
set(EMBED_FILES
    "certs/root-CA.crt"
    "certs/coop_snooper.cert.pem"
    "certs/coop_snooper.private.key"
)

# Add the source files to the build
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDES}
#     EMBED_TXTFILES ${EMBED_FILES}
)

target_add_binary_data(${COMPONENT_TARGET} "certs/root-CA.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/coop-snooper.cert.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/coop-snooper.private.key" TEXT)
