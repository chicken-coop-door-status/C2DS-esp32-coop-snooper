# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Diagnostic messages
message(STATUS "IDF_PATH: $ENV{IDF_PATH}")
message(STATUS "CMake version: ${CMAKE_VERSION}")

# Define the source files
set(SOURCES 
    "main.c" 
    "mp3.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Get all the component include directories
file(GLOB_RECURSE COMPONENT_SOURCES "components/*/*.c")

file(GLOB_RECURSE COMPONENT_INCLUDES "components/*/include")

# Print sdkconfig variables
message(STATUS "CONFIG_TENNIS_HOUSE: ${CONFIG_TENNIS_HOUSE}")
message(STATUS "CONFIG_FARM_HOUSE: ${CONFIG_FARM_HOUSE}")

# Determine which configuration is selected
if(CONFIG_TENNIS_HOUSE)
    add_definitions(-DTENNIS_HOUSE)
    add_definitions(-DWIFI_SSID="${CONFIG_TENNIS_HOUSE_WIFI_SSID}")
    add_definitions(-DWIFI_PASSWORD="${CONFIG_TENNIS_HOUSE_WIFI_PASSWORD}")
    add_definitions(-DWIFI_HOSTNAME="${CONFIG_TENNIS_HOUSE_WIFI_HOSTNAME}")
    add_definitions(-DOTA_URL="${CONFIG_TENNIS_HOUSE_OTA_URL}")
    set(CERT_SOURCE "certs/coop_snooper_tennis_home_certificate_pem.c")
    set(KEY_SOURCE "certs/coop_snooper_tennis_home_private_pem_key.c")
    message(STATUS "------------------------>Configuration: TENNIS_HOUSE")
elseif(CONFIG_FARM_HOUSE)
    add_definitions(-DFARM_HOUSE)
    add_definitions(-DWIFI_SSID="${CONFIG_FARM_HOUSE_WIFI_SSID}")
    add_definitions(-DWIFI_PASSWORD="${CONFIG_FARM_HOUSE_WIFI_PASSWORD}")
    add_definitions(-DWIFI_HOSTNAME="${CONFIG_FARM_HOUSE_WIFI_HOSTNAME}")
    add_definitions(-DOTA_URL="${CONFIG_FARM_HOUSE_OTA_URL}")
    set(CERT_SOURCE "certs/coop_snooper_farmhouse_certificate_pem.c")
    set(KEY_SOURCE "certs/coop_snooper_farmhouse_private_pem_key.c")
    message(STATUS "------------------------>Configuration: FARM_HOUSE")
else()
    message(STATUS "++++++++++++++++++++++++Configuration: UNKNOWN+++++++++++++++++++++++++")
endif()

set(AWS_SOURCE "certs/AmazonRootCA1_pem.c")

# Add the source files to the build
idf_component_register(
    SRCS 
        ${SOURCES}
        ${COMPONENT_SOURCES}
        ${CERT_SOURCE}
        ${KEY_SOURCE}
        ${AWS_SOURCE}
    INCLUDE_DIRS 
        ${INCLUDES}
        ${COMPONENT_INCLUDES}
    REQUIRES
        nvs_flash
        mbedtls 
        app_update 
        esp_https_ota 
        mqtt 
        json
        esp_netif
        esp_wifi
    PRIV_REQUIRES 
        gecl-ota-manager
        gecl-wifi-manager
        gecl-rgb-led-manager
        gecl-time-sync-manager
        gecl-logger-manager
        gecl-mqtt-manager
        gecl-misc-util-manager
)
