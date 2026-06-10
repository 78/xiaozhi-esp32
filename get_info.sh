#!/bin/bash

# ============================================
# Script de Diagnóstico para XiaoZhi AI ESP32-C6
# ============================================

OUTPUT_FILE="xiaozhi_diagnostico.txt"

echo "========================================" | tee $OUTPUT_FILE
echo "DIAGNÓSTICO DEL PROYECTO XIAOZHI AI" | tee -a $OUTPUT_FILE
echo "Fecha: $(date)" | tee -a $OUTPUT_FILE
echo "========================================" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 1. Información del Sistema
echo "=== 1. INFORMACIÓN DEL SISTEMA ===" | tee -a $OUTPUT_FILE
echo "Sistema Operativo: $(lsb_release -ds 2>/dev/null || cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)" | tee -a $OUTPUT_FILE
echo "Arquitectura: $(uname -m)" | tee -a $OUTPUT_FILE
echo "Kernel: $(uname -r)" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 2. Versiones de Python
echo "=== 2. VERSIONES DE PYTHON ===" | tee -a $OUTPUT_FILE
echo "Python3 versión: $(python3 --version 2>&1)" | tee -a $OUTPUT_FILE
echo "Python3 path: $(which python3)" | tee -a $OUTPUT_FILE
echo "Python3 pip versión: $(pip3 --version 2>&1)" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 3. Entorno ESP-IDF
echo "=== 3. ENTORNO ESP-IDF ===" | tee -a $OUTPUT_FILE
if [ -d ~/esp/esp-idf ]; then
    echo "ESP-IDF Path: ~/esp/esp-idf" | tee -a $OUTPUT_FILE
    cd ~/esp/esp-idf
    echo "ESP-IDF Versión: $(git describe --tags 2>/dev/null || echo 'No git tag')" | tee -a $OUTPUT_FILE
    echo "ESP-IDF Branch: $(git branch --show-current 2>/dev/null || echo 'No branch')" | tee -a $OUTPUT_FILE
    echo "ESP-IDF Commit: $(git rev-parse --short HEAD 2>/dev/null || echo 'No commit')" | tee -a $OUTPUT_FILE
else
    echo "ESP-IDF NO ENCONTRADO en ~/esp/esp-idf" | tee -a $OUTPUT_FILE
fi
echo "" | tee -a $OUTPUT_FILE

# 4. Herramientas ESP-IDF
echo "=== 4. HERRAMIENTAS ESP-IDF ===" | tee -a $OUTPUT_FILE
if [ -f ~/.espressif/python_env/idf5.4_py3.13_env/bin/python ]; then
    echo "Python venv ESP-IDF: ~/.espressif/python_env/idf5.4_py3.13_env/bin/python" | tee -a $OUTPUT_FILE
    ~/.espressif/python_env/idf5.4_py3.13_env/bin/python --version 2>&1 | tee -a $OUTPUT_FILE
fi
echo "esptool.py versión: $(esptool.py --version 2>&1 | head -1)" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 5. Proyecto XiaoZhi AI
echo "=== 5. PROYECTO XIAOZHI AI ===" | tee -a $OUTPUT_FILE
cd ~/src/xiaozhi-esp32 2>/dev/null || cd /media/optimus/FILES/src/xiaozhi-esp32 2>/dev/null || echo "No se encuentra el proyecto"
PROJECT_DIR=$(pwd)
echo "Directorio del proyecto: $PROJECT_DIR" | tee -a $OUTPUT_FILE
echo "Rama actual: $(git branch --show-current 2>/dev/null || echo 'No git')" | tee -a $OUTPUT_FILE
echo "Commit actual: $(git rev-parse --short HEAD 2>/dev/null || echo 'No commit')" | tee -a $OUTPUT_FILE
echo "Último commit: $(git log -1 --format=%s 2>/dev/null || echo 'No log')" | tee -a $OUTPUT_FILE
echo "Versión en CMakeLists.txt: $(grep "project(xiaozhi)" CMakeLists.txt -A 2 | grep "VERSION" | head -1 || echo 'No version found')" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 6. Configuración del proyecto (sdkconfig)
echo "=== 6. CONFIGURACIÓN DEL PROYECTO (sdkconfig) ===" | tee -a $OUTPUT_FILE
if [ -f sdkconfig ]; then
    echo "IDF Target: $(grep CONFIG_IDF_TARGET= sdkconfig | head -1)" | tee -a $OUTPUT_FILE
    echo "Board Type: $(grep CONFIG_BOARD_TYPE_ sdkconfig | grep =y | head -1)" | tee -a $OUTPUT_FILE
    echo "Flash Size: $(grep CONFIG_ESPTOOLPY_FLASHSIZE= sdkconfig | head -1)" | tee -a $OUTPUT_FILE
    echo "Partition Table: $(grep CONFIG_PARTITION_TABLE_CUSTOM_FILENAME= sdkconfig | head -1)" | tee -a $OUTPUT_FILE
    echo "Audio Enabled: $(grep CONFIG_USE_AUDIO_ sdkconfig | head -3)" | tee -a $OUTPUT_FILE
    echo "Wake Word: $(grep CONFIG_USE_.*WAKE_WORD= sdkconfig)" | tee -a $OUTPUT_FILE
else
    echo "sdkconfig NO ENCONTRADO" | tee -a $OUTPUT_FILE
fi
echo "" | tee -a $OUTPUT_FILE

# 7. Hardware detectado (puertos USB)
echo "=== 7. PUERTOS USB DETECTADOS ===" | tee -a $OUTPUT_FILE
ls -la /dev/ttyUSB* 2>/dev/null | tee -a $OUTPUT_FILE
ls -la /dev/ttyACM* 2>/dev/null | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 8. Errores recientes de compilación
echo "=== 8. ÚLTIMOS ERRORES DE COMPILACIÓN ===" | tee -a $OUTPUT_FILE
if [ -f build/log/idf_py_stderr_output_* ]; then
    echo "Últimos errores encontrados:" | tee -a $OUTPUT_FILE
    tail -50 build/log/idf_py_stderr_output_* 2>/dev/null | tee -a $OUTPUT_FILE
else
    echo "No se encontraron logs de error" | tee -a $OUTPUT_FILE
fi
echo "" | tee -a $OUTPUT_FILE

# 9. Dependencias del proyecto
echo "=== 9. DEPENDENCIAS DEL PROYECTO ===" | tee -a $OUTPUT_FILE
echo "Managed components:" | tee -a $OUTPUT_FILE
ls managed_components/ 2>/dev/null | head -20 | tee -a $OUTPUT_FILE
echo "Total componentes: $(ls managed_components/ 2>/dev/null | wc -l)" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

# 10. Resumen del problema
echo "=== 10. DESCRIPCIÓN DEL PROBLEMA ===" | tee -a $OUTPUT_FILE
echo "Error principal: undefined reference to create_board()" | tee -a $OUTPUT_FILE
echo "Causa: La función create_board() no está implementada para la placa seleccionada" | tee -a $OUTPUT_FILE
echo "Placa objetivo: ESP32-C6-DevKitC-1" | tee -a $OUTPUT_FILE
echo "Board Type seleccionado: Placa de pruebas (WiFi) ESP32-C6" | tee -a $OUTPUT_FILE
echo "" | tee -a $OUTPUT_FILE

echo "========================================" | tee -a $OUTPUT_FILE
echo "DIAGNÓSTICO COMPLETADO" | tee -a $OUTPUT_FILE
echo "Archivo guardado en: $OUTPUT_FILE" | tee -a $OUTPUT_FILE
echo "========================================" | tee -a $OUTPUT_FILE

