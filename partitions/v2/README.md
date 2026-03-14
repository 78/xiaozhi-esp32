# Version 2 Partition Table

This version introduces significant improvements over v1 by adding an `assets` partition to support network-loadable content and optimizing partition layouts for different flash sizes.

## Key Changes from v1

### Major Improvements
1. **Added Assets Partition**: New `assets` partition for network-loadable content
2. **Replaced Model Partition**: The old `model` partition (960KB) is replaced with a larger `assets` partition
3. **Optimized App Partitions**: Reduced application partition sizes to accommodate assets
4. **Enhanced Flexibility**: Support for dynamic content updates without reflashing

### Assets Partition Features
The `assets` partition stores:
- **Wake word models**: Customizable wake word models that can be loaded from the network
- **Theme files**: Complete theming system including:
  - Fonts (text and icon fonts)
  - Audio effects and sound files
  - Background images and UI elements
  - Custom emoji packs
  - Language configuration files
- **Dynamic Content**: All content can be updated over-the-air via HTTP downloads

## Partition Layout Comparison

### v1 Layout (16MB)
- `nvs`: 16KB (non-volatile storage)
- `otadata`: 8KB (OTA data)
- `phy_init`: 4KB (PHY initialization data)
- `model`: 960KB (model storage - fixed content)
- `ota_0`: 6MB (application partition 0)
- `ota_1`: 6MB (application partition 1)

### v2 Layout (16MB)
- `nvs`: 16KB (non-volatile storage)
- `otadata`: 8KB (OTA data)
- `phy_init`: 4KB (PHY initialization data)
- `ota_0`: 4MB (application partition 0)
- `ota_1`: 4MB (application partition 1)
- `assets`: 8MB (network-loadable assets)

## Available Configurations

### 8MB Flash Devices (`8m.csv`)
- `nvs`: 16KB
- `otadata`: 8KB
- `phy_init`: 4KB
- `ota_0`: 3MB
- `ota_1`: 3MB
- `assets`: 2MB

### 16MB Flash Devices (`16m.csv`) - Standard
- `nvs`: 16KB
- `otadata`: 8KB
- `phy_init`: 4KB
- `ota_0`: 4MB
- `ota_1`: 4MB
- `assets`: 8MB

### 16MB Flash Devices (`16m_c3.csv`) - ESP32-C3 Optimized
- `nvs`: 16KB
- `otadata`: 8KB
- `phy_init`: 4KB
- `ota_0`: 4MB
- `ota_1`: 4MB
- `assets`: 4MB (4000K - limited by available mmap pages)

### 32MB Flash Devices (`32m.csv`)
- `nvsfactory`: 200KB
- `nvs`: 840KB
- `otadata`: 8KB
- `phy_init`: 4KB
- `ota_0`: 4MB
- `ota_1`: 4MB
- `assets`: 16MB

## Benefits

1. **Dynamic Content Management**: Users can download and update wake word models, themes, and other assets without reflashing the device
2. **Reduced App Size**: Application partitions are optimized, allowing more space for dynamic content
3. **Enhanced Customization**: Support for custom themes, wake words, and language packs enhances user experience
4. **Network Flexibility**: Assets can be updated independently of the main application firmware
5. **Better Resource Utilization**: Efficient use of flash memory with configurable asset storage
6. **OTA Asset Updates**: Assets can be updated over-the-air via HTTP downloads

## Technical Details

- **Partition Type**: Assets partition uses `spiffs` subtype for SPIFFS filesystem compatibility
- **Memory Mapping**: Assets are memory-mapped for efficient access during runtime
- **Checksum Validation**: Built-in integrity checking ensures asset data validity
- **Progressive Download**: Assets can be downloaded progressively with progress tracking
- **Fallback Support**: Graceful fallback to default assets if network updates fail

## Migration from v1

When upgrading from v1 to v2:
1. **Backup Important Data**: Ensure any important data in the old `model` partition is backed up
2. **Flash New Partition Table**: Use the appropriate v2 partition table for your flash size
3. **Download Assets**: The device will automatically download required assets on first boot
4. **Verify Functionality**: Ensure all features work correctly with the new partition layout

## Usage Notes

- The `assets` partition size varies by configuration to optimize for different flash sizes
- ESP32-C3 devices use a smaller assets partition (4MB) due to limited available mmap pages in the system
- 32MB devices get the largest assets partition (16MB) for maximum content storage
- All partition tables maintain proper alignment for optimal flash performance 