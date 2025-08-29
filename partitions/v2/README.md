# Version 2 Partition Table

This version introduces significant improvements over v1 by adding an `assets` partition to support network-loadable content.

## Key Changes from v1

### Added Assets Partition
The v2 partition table includes a new `assets` partition that stores:
- **Wake word models**: Customizable wake word models that can be loaded from the network
- **Theme files**: Complete theming system including:
  - Fonts
  - Audio effects
  - Background images
  - Custom emoji packs

### Partition Layout Comparison

#### v1 Layout (16MB)
- `nvs`: 16KB (non-volatile storage)
- `otadata`: 8KB (OTA data)
- `phy_init`: 4KB (PHY initialization data)
- `model`: 960KB (model storage)
- `ota_0`: 6MB (application partition 0)
- `ota_1`: 6MB (application partition 1)

#### v2 Layout (16MB)
- `nvs`: 16KB (non-volatile storage)
- `otadata`: 8KB (OTA data)
- `phy_init`: 4KB (PHY initialization data)
- `model`: 960KB (model storage)
- `ota_0`: 4MB (application partition 0)
- `ota_1`: 4MB (application partition 1)
- `assets`: 7MB (network-loadable assets)

### Benefits

1. **Dynamic Content**: Users can download and update wake word models and themes without reflashing
2. **Reduced App Size**: Application partitions are smaller, allowing more space for assets
3. **Customization**: Support for custom themes and wake words enhances user experience
4. **Network Flexibility**: Assets can be updated independently of the main application

### Available Configurations

- `8m.csv`: For 8MB flash devices
- `16m.csv`: For 16MB flash devices (standard)
- `16m_c3.csv`: For 16MB flash devices with ESP32-C3 optimization 