
cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "src/gdey/GxEPD2_420_GDEY042T81.cpp" "src/GxEPD2_EPD.cpp" "src/GT30l32s4w/driver_gt30l32s4w_basic.c" "src/GT30l32s4w/driver_gt30l32s4w.c" 
                      "src/GT30l32s4w/driver_gt30l32s4w_interface_template.cpp" "src/GT30l32s4w/utf8_to_gb2312_table.c""src/DrawMixedString.cc"
                       INCLUDE_DIRS ".""src" "examples/GxEPD2_HelloWorld" "src/GT30l32s4w"
                       REQUIRES arduino-esp32 Adafruit-GFX)

project(GxEPD2)
