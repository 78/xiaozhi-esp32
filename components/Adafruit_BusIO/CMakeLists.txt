# Adafruit Bus IO Library
# https://github.com/adafruit/Adafruit_BusIO
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "Adafruit_I2CDevice.cpp" "Adafruit_BusIO_Register.cpp" "Adafruit_SPIDevice.cpp" "Adafruit_GenericDevice.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES arduino-esp32)

project(Adafruit_BusIO)
