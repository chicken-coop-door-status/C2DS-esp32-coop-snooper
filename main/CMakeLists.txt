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
    "logging.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Determine which configuration is selected
if(DEFINED CONFIG_TENNIS_HOUSE)
    add_definitions(-DTENNIS_HOUSE)
    add_definitions(-DWIFI_SSID=\"${CONFIG_TENNIS_HOUSE_WIFI_SSID}\")
    add_definitions(-DWIFI_PASSWORD=\"${CONFIG_TENNIS_HOUSE_WIFI_PASSWORD}\")
    add_definitions(-DWIFI_HOSTNAME=\"${CONFIG_TENNIS_HOUSE_WIFI_HOSTNAME}\")
    set(CERT_SOURCE "certs/coop_snooper_tennis_home_certificate_pem.c")
    set(KEY_SOURCE "certs/coop_snooper_tennis_home_private_pem_key.c")
elseif(DEFINED CONFIG_FARM_HOUSE)
    add_definitions(-DFARM_HOUSE)
    add_definitions(-DWIFI_SSID=\"${CONFIG_FARM_HOUSE_WIFI_SSID}\")
    add_definitions(-DWIFI_PASSWORD=\"${CONFIG_FARM_HOUSE_WIFI_PASSWORD}\")
    add_definitions(-DWIFI_HOSTNAME=\"${CONFIG_FARM_HOUSE_WIFI_HOSTNAME}\")
    set(CERT_SOURCE "certs/coop_snooper_farmhouse_certificate_pem.c")
    set(KEY_SOURCE "certs/coop_snooper_farmhouse_private_pem_key.c")
endif()

set(AWS_SOURCE "certs/AmazonRootCA1_pem.c")

# Add the source files to the build
idf_component_register(
    SRCS 
        ${SOURCES}
        ${CERT_SOURCE}
        ${KEY_SOURCE}
        ${AWS_SOURCE}
    INCLUDE_DIRS 
        ${INCLUDES}
)
