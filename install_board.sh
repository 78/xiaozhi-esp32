#!/usr/bin/env bash
# =============================================================================
#  install_board.sh — Instala la placa ESP32-C6-DevKitC-1 en XiaoZhi AI
# =============================================================================
#
#  Uso:
#    cd /home/optimus/src/xiaozhi-esp32   ← directorio raíz del proyecto
#    bash /ruta/a/install_board.sh
#
#  Qué hace:
#    1. Copia los archivos de la placa a main/boards/bread-compact-wifi-esp32c6/
#    2. Copia la tabla de particiones 8m.csv a partitions/v1/
#    3. Corrige sdkconfig (flash 16MB→8MB, partición 16m→8m)
#    4. Parchea main/CMakeLists.txt para incluir la carpeta de la placa
#    5. Verifica que Kconfig.projbuild tenga la entrada de la placa
#    6. Limpia build/ para forzar recompilación limpia
#
# =============================================================================
set -e

# ── Colores ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ── Verificar directorio ──────────────────────────────────────────────────────
if [[ ! -f "CMakeLists.txt" ]] || [[ ! -d "main" ]]; then
    error "Ejecutar desde la raíz del proyecto xiaozhi-esp32"
fi

PROJECT_ROOT="$(pwd)"
BOARD_DIR="$PROJECT_ROOT/main/boards/bread-compact-wifi-esp32c6"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}  XiaoZhi AI — Instalador placa ESP32-C6-DevKitC-1     ${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo ""

# ── 1. Crear carpeta de la placa ──────────────────────────────────────────────
info "Creando carpeta de la placa..."
mkdir -p "$BOARD_DIR"
ok "Carpeta: $BOARD_DIR"

# ── 2. Copiar archivos de la placa ────────────────────────────────────────────
info "Copiando archivos de la placa..."

FILES=(
    "board-files/esp32_c6_devkitc1.cc"
    "board-files/board_config.h"
    "board-files/CMakeLists.txt"
)

for f in "${FILES[@]}"; do
    src="$SCRIPT_DIR/$f"
    dst="$BOARD_DIR/$(basename "$f")"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        ok "  → $(basename "$f")"
    else
        error "Archivo fuente no encontrado: $src"
    fi
done

# ── 3. Tabla de particiones 8MB ──────────────────────────────────────────────
PART_DIR="$PROJECT_ROOT/partitions/v1"
mkdir -p "$PART_DIR"
info "Copiando tabla de particiones 8m.csv..."

if [[ -f "$SCRIPT_DIR/partitions/8m.csv" ]]; then
    cp "$SCRIPT_DIR/partitions/8m.csv" "$PART_DIR/8m.csv"
    ok "Tabla: $PART_DIR/8m.csv"
else
    # Si no existe el archivo fuente, crear uno básico
    warn "No se encontró partitions/8m.csv, generando tabla básica..."
    cat > "$PART_DIR/8m.csv" << 'PARTCSV'
# Tabla de particiones 8MB — ESP32-C6-DevKitC-1
# Name,     Type, SubType,  Offset,   Size,     Flags
nvs,          data, nvs,      0x9000,   0x6000,
phy_init,     data, phy,      0xF000,   0x1000,
factory,      app,  factory,  0x10000,  0x300000,
ota_0,        app,  ota_0,    0x310000, 0x300000,
ota_1,        app,  ota_1,    0x610000, 0x300000,
ota_data,     data, ota,      0x10000,  0x2000,
model,        data, spiffs,   0x712000, 0x80000,
storage,      data, spiffs,   0x792000, 0x6E000,
PARTCSV
    ok "Tabla generada: $PART_DIR/8m.csv"
fi

# ── 4. Verificar / parchear main/CMakeLists.txt ───────────────────────────────
info "Verificando main/CMakeLists.txt..."
MAIN_CMAKE="$PROJECT_ROOT/main/CMakeLists.txt"

if [[ ! -f "$MAIN_CMAKE" ]]; then
    error "No se encontró $MAIN_CMAKE"
fi

# Buscar si ya incluye la carpeta de la placa
if grep -q "bread-compact-wifi-esp32c6" "$MAIN_CMAKE"; then
    ok "main/CMakeLists.txt ya referencia la carpeta de la placa"
else
    warn "main/CMakeLists.txt NO referencia la placa — agregando..."
    # Intentar agregar después de la línea de boards/ o al final de SRCS
    # Estrategia: buscar la línea donde se agregan otras carpetas de placa
    if grep -q "BOARD_TYPE" "$MAIN_CMAKE"; then
        # Añadir una entrada condicional al final del archivo
        cat >> "$MAIN_CMAKE" << 'CMAKEPATCH'

# ── Placa ESP32-C6-DevKitC-1 (agregado por install_board.sh) ──────────────
if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_ESP32C6)
    set(BOARD_SRCS ${BOARD_SRCS}
        "boards/bread-compact-wifi-esp32c6/esp32_c6_devkitc1.cc"
    )
    set(BOARD_INCLUDE_DIRS ${BOARD_INCLUDE_DIRS}
        "boards/bread-compact-wifi-esp32c6"
    )
endif()
# ────────────────────────────────────────────────────────────────────────────
CMAKEPATCH
        ok "Parche agregado a main/CMakeLists.txt"
    else
        warn "No se pudo parchear automáticamente main/CMakeLists.txt"
        warn "Agregar manualmente la referencia a la carpeta de la placa"
    fi
fi

# ── 5. Corregir sdkconfig si existe ──────────────────────────────────────────
SDKCONFIG="$PROJECT_ROOT/sdkconfig"
if [[ -f "$SDKCONFIG" ]]; then
    info "Corrigiendo sdkconfig (flash 16MB→8MB, partición 16m→8m)..."

    # Flash size
    if grep -q 'CONFIG_ESPTOOLPY_FLASHSIZE="16MB"' "$SDKCONFIG"; then
        sed -i 's/CONFIG_ESPTOOLPY_FLASHSIZE="16MB"/CONFIG_ESPTOOLPY_FLASHSIZE="8MB"/' "$SDKCONFIG"
        ok "  Flash: 16MB → 8MB"
    else
        warn "  Flash ya es 8MB o no se encontró la línea (OK)"
    fi

    # Tabla de particiones
    if grep -q '16m.csv' "$SDKCONFIG"; then
        sed -i 's|partitions/v1/16m.csv|partitions/v1/8m.csv|' "$SDKCONFIG"
        ok "  Partición: 16m.csv → 8m.csv"
    else
        warn "  Tabla de particiones ya usa 8m.csv o no encontrada (OK)"
    fi

    # Verificar board type
    if ! grep -q 'CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_ESP32C6=y' "$SDKCONFIG"; then
        warn "sdkconfig no tiene CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_ESP32C6=y"
        warn "Ejecutar: idf.py menuconfig → Xiaozhi Assistant → Board Type → Placa de pruebas (WiFi) ESP32-C6"
    else
        ok "  Board type: BREAD_COMPACT_WIFI_ESP32C6 ✓"
    fi
else
    warn "sdkconfig no encontrado — se generará en el primer build"
fi

# ── 6. Limpiar build ─────────────────────────────────────────────────────────
info "Limpiando build/ para recompilación limpia..."
if [[ -d "$PROJECT_ROOT/build" ]]; then
    rm -rf "$PROJECT_ROOT/build"
    ok "build/ eliminado"
fi

# También limpiar sdkconfig para evitar conflictos
# (comentar si se quiere preservar la configuración)
# rm -f "$SDKCONFIG"

# ── Resumen ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}  ✓ Instalación completada${NC}"
echo -e "${BOLD}═══════════════════════════════════════════════════════${NC}"
echo ""
echo -e "Archivos instalados en:"
echo -e "  ${CYAN}$BOARD_DIR/${NC}"
echo -e "    ├── esp32_c6_devkitc1.cc  (implementa create_board)"
echo -e "    ├── board_config.h        (pines y parámetros)"
echo -e "    └── CMakeLists.txt        (fuentes del componente)"
echo ""
echo -e "  ${CYAN}$PART_DIR/8m.csv${NC}  (tabla de particiones 8MB)"
echo ""
echo -e "Próximos pasos:"
echo -e "  ${BOLD}1.${NC} Verificar Kconfig.projbuild (ver Kconfig_fragment.txt)"
echo -e "  ${BOLD}2.${NC} cd $PROJECT_ROOT"
echo -e "  ${BOLD}3.${NC} idf.py set-target esp32c6"
echo -e "  ${BOLD}4.${NC} idf.py menuconfig  ← seleccionar placa si es necesario"
echo -e "  ${BOLD}5.${NC} idf.py build"
echo -e "  ${BOLD}6.${NC} idf.py -p /dev/ttyUSB0 flash monitor"
echo ""
echo -e "${YELLOW}Pines I2C del OLED SSD1306:${NC}"
echo -e "  SDA → GPIO6   SCL → GPIO7   Addr: 0x3C"
echo ""
echo -e "${YELLOW}Micrófono INMP441 (sin conectar):${NC}"
echo -e "  Para habilitarlo: descomentar CONFIG_USE_INMP441_MIC"
echo -e "  en esp32_c6_devkitc1.cc y conectar WS=GPIO4 SCK=GPIO5 SD=GPIO3"
echo ""
