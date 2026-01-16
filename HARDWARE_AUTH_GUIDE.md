# Hardware Authentication & Security Implementation Guide

## Overview

This implementation provides Flipper Zero-style hardware authentication to prevent your firmware from running on clone devices. It uses:

1. **eFuse-based device identity** - Burned during manufacturing
2. **Digital signatures** - ECDSA P-256 signatures verify authenticity
3. **Secure Boot** - Prevents modified firmware from running
4. **Flash Encryption** - Protects firmware and data from extraction
5. **Certificate Management** - Using `esp_secure_cert_mgr`

## Architecture

```
┌─────────────────────────────────────────────────┐
│          uFlake Kernel Initialization           │
├─────────────────────────────────────────────────┤
│  1. Initialize Crypto Engine                    │
│  2. Initialize Hardware Authentication          │
│  3. Verify Device Authenticity                  │
│     ├─ Read eFuse identity                      │
│     ├─ Verify digital signature                 │
│     ├─ Check hardware version                   │
│     └─ Validate certificate                     │
│  4. Check Secure Boot & Flash Encryption        │
│  5. Continue boot OR halt if clone detected     │
└─────────────────────────────────────────────────┘
```

## Step 1: Enable Secure Boot & Flash Encryption

### For ESP32-S3, add to `sdkconfig.defaults.esp32s3`:

```ini
# ============================================
# SECURE BOOT CONFIGURATION
# ============================================
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SUPPORTS_RSA=y
CONFIG_SECURE_BOOTLOADER_MODE_RELEASE=y
CONFIG_SECURE_BOOT_ENABLE_AGGRESSIVE_KEY_REVOKE=y

# Sign images during build
CONFIG_SECURE_SIGNED_APPS_SCHEME=RSA_PSS
CONFIG_SECURE_SIGNED_ON_BUILD=y

# Path to your signing key (KEEP THIS SECRET!)
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"

# ============================================
# FLASH ENCRYPTION CONFIGURATION
# ============================================
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
CONFIG_SECURE_FLASH_REQUIRE_FULLY_ENCRYPTED=y
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_ENC=n
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_DEC=n
CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_CACHE=n

# Encryption key protection
CONFIG_SECURE_FLASH_ENCRYPTION_AES256=y

# ============================================
# eFUSE PROTECTION
# ============================================
CONFIG_EFUSE_SECURE_VERSION_ENABLED=y
CONFIG_EFUSE_VIRTUAL=n

# ============================================
# BOOTLOADER SECURITY
# ============================================
CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y
CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP=n
CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC=y
CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC_SIZE=0

# Disable JTAG (important for production!)
CONFIG_ESP_SYSTEM_DISABLE_JTAG=y

# Disable ROM UART downloads in production
CONFIG_ESP_SYSTEM_DISABLE_ROM_DOWNLOAD=y
```

## Step 2: Generate Signing Keys

### Generate Secure Boot Key (RSA-PSS):
```bash
cd /path/to/your/project

# Generate RSA-3072 key for secure boot
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem

# CRITICAL: Back up this key securely! If lost, you can't update devices!
# Keep this file SECRET - anyone with this can sign fake firmware
```

### Generate Device Authentication Key (ECDSA P-256):
```bash
# Generate private key for device identity signing
openssl ecparam -name prime256v1 -genkey -noout -out device_identity_private_key.pem

# Extract public key (this goes in your firmware)
openssl ec -in device_identity_private_key.pem -pubout -out device_identity_public_key.pem

# Convert public key to C array format
xxd -i device_identity_public_key.pem > device_public_key.h
```

## Step 3: Device Provisioning (Manufacturing)

During manufacturing, each device must be provisioned with a unique identity:

```c
#include "kernel.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"

void provision_device(uint32_t device_number, const char *serial)
{
    hw_identity_t identity = {0};
    
    // 1. Generate unique device ID
    uflake_hw_get_unique_id(identity.device_id, 16);
    
    // 2. Set serial number
    snprintf((char*)identity.serial_number, 32, "%s-%06lu", serial, device_number);
    
    // 3. Set hardware info
    identity.hw_version = 1;
    identity.board_revision = 1;
    identity.manufacture_date = time(NULL); // Current Unix timestamp
    
    // 4. Calculate signature using your PRIVATE key (keep offline!)
    // This step should be done on a secure offline computer
    uint8_t hash[32];
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);
    
    mbedtls_sha256_update(&sha_ctx, identity.device_id, sizeof(identity.device_id));
    mbedtls_sha256_update(&sha_ctx, identity.serial_number, sizeof(identity.serial_number));
    mbedtls_sha256_update(&sha_ctx, &identity.hw_version, 1);
    mbedtls_sha256_update(&sha_ctx, &identity.board_revision, 1);
    mbedtls_sha256_update(&sha_ctx, (uint8_t*)&identity.manufacture_date, 4);
    
    mbedtls_sha256_finish(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);
    
    // Sign with your private key (load from device_identity_private_key.pem)
    // ... ECDSA signing code here ...
    // Result goes in identity.signature
    
    // 5. Burn to eFuse (PERMANENT - CANNOT BE CHANGED!)
    ESP_LOGW("PROVISION", "!!! About to burn device identity to eFuse !!!");
    ESP_LOGW("PROVISION", "!!! This is PERMANENT and CANNOT be undone !!!");
    ESP_LOGW("PROVISION", "Serial: %s", identity.serial_number);
    
    // Uncomment when ready to provision:
    // uflake_hw_provision_device(&identity, NULL);
    
    ESP_LOGI("PROVISION", "Device provisioned successfully");
}
```

## Step 4: Integrate into Kernel

Update `kernel.c` to check authentication on boot:

```c
#include "kernel.h"

void handle_clone_detection(hw_auth_status_t status)
{
    ESP_LOGE("KERNEL", "!!! CLONE HARDWARE DETECTED !!!");
    ESP_LOGE("KERNEL", "This firmware is licensed for genuine hardware only.");
    ESP_LOGE("KERNEL", "Authentication status: %d", status);
    
    // Options for handling clones:
    
    // OPTION 1: Hard fail - don't boot at all
    // esp_restart();
    
    // OPTION 2: Limited functionality mode
    // disable_advanced_features();
    // show_warning_screen();
    
    // OPTION 3: Log and continue (for development)
    ESP_LOGW("KERNEL", "Continuing in development mode...");
}

uflake_result_t uflake_kernel_init(void)
{
    // ... existing initialization ...
    
    // Initialize hardware authentication
    ESP_LOGI(TAG, "Initializing hardware authentication...");
    uflake_hw_auth_register_callback(handle_clone_detection);
    
    if (uflake_hw_auth_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Hardware authentication init failed");
        return UFLAKE_ERROR;
    }
    
    // Check if device is genuine
    if (!uflake_hw_is_genuine())
    {
        ESP_LOGW(TAG, "Running on non-genuine hardware");
        // Handle according to your policy
    }
    else
    {
        ESP_LOGI(TAG, "✓ Genuine hardware verified");
    }
    
    // ... continue initialization ...
}
```

## Step 5: Build and Flash with Encryption

### First Flash (enables security):
```bash
# Build the project
idf.py build

# Flash with encryption and secure boot (FIRST TIME ONLY)
idf.py -p COM3 flash monitor

# On first boot, the bootloader will:
# 1. Enable flash encryption
# 2. Encrypt all flash partitions
# 3. Burn encryption keys to eFuse
# 4. Enable secure boot
# THIS PROCESS IS PERMANENT!
```

### Subsequent Updates:
```bash
# For encrypted devices, use:
idf.py -p COM3 encrypted-flash monitor

# Or build and use external tools:
idf.py build
espsecure.py encrypt_flash_data --aes_xts --address 0x10000 \
    build/uFlake.bin build/uFlake_encrypted.bin
esptool.py write_flash 0x10000 build/uFlake_encrypted.bin
```

## Security Levels

### Level 1: Basic (Development)
- Hardware authentication enabled
- Logs warnings on clone detection
- Allows continued operation
```c
#define SECURITY_LEVEL 1
```

### Level 2: Restricted (Beta/Testing)
- Hardware authentication required
- Limited features on clone devices
- Full logging enabled
```c
#define SECURITY_LEVEL 2
```

### Level 3: Production (Release)
- Hardware authentication required
- Secure boot enabled
- Flash encryption enabled
- Refuses to boot on clone hardware
- Minimal logging
```c
#define SECURITY_LEVEL 3
```

## Alternative: Using esp_secure_cert_mgr

The `esp_secure_cert_mgr` component provides a higher-level certificate management system:

### Add to your component dependencies:
```yaml
dependencies:
  espressif/esp_secure_cert_mgr: ^2.8.0
```

### Partition table (`partitions.csv`):
```csv
# Name,     Type, SubType, Offset,   Size, Flags
nvs,        data, nvs,     0x9000,   24K,
otadata,    data, ota,     0xF000,   8K,
phy_init,   data, phy,     0x11000,  4K,
# Certificate partition
esp_secure_cert, data, 0x3F, 0x12000, 4K,
factory,    app,  factory, 0x20000,  1M,
ota_0,      app,  ota_0,   0x120000, 1M,
ota_1,      app,  ota_1,   0x220000, 1M,
```

### Use certificates:
```c
#include "esp_secure_cert_read.h"

void verify_device_certificate(void)
{
    char *cert = NULL;
    size_t cert_len = 0;
    
    // Read device certificate
    esp_err_t err = esp_secure_cert_get_device_cert(&cert, &cert_len);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Device certificate loaded (%d bytes)", cert_len);
        // Verify certificate chain here
    }
}
```

## Testing Your Implementation

### Test 1: Verify eFuse Reading
```c
uint8_t unique_id[16];
uflake_hw_get_unique_id(unique_id, 16);
ESP_LOGI("TEST", "Device ID: " LOG_FMT_HEX, LOG_HEX_ARRAY(unique_id, 16));
```

### Test 2: Check Security Status
```c
bool secure_boot = uflake_efuse_is_secure_boot_enabled();
bool flash_enc = uflake_efuse_is_flash_encryption_enabled();
ESP_LOGI("TEST", "Secure Boot: %s", secure_boot ? "ON" : "OFF");
ESP_LOGI("TEST", "Flash Encryption: %s", flash_enc ? "ON" : "OFF");
```

### Test 3: Authentication
```c
hw_auth_status_t status = uflake_hw_auth_verify();
ESP_LOGI("TEST", "Auth Status: %d", status);
```

## Important Warnings

⚠️ **CRITICAL SECURITY NOTES:**

1. **eFuse burns are PERMANENT** - Test thoroughly before production
2. **Keep signing keys SECURE** - Store offline, encrypted, with backups
3. **Test on sacrificial device first** - You can brick devices if misconfigured
4. **Cannot downgrade security** - Once enabled, secure boot/encryption stay forever
5. **Plan for key revocation** - ESP32-S3 supports up to 3 key revocations
6. **Backup everything** - Keys, certificates, provisioning scripts

## Comparison with Flipper Zero

| Feature | Flipper Zero | Your uFlake |
|---------|--------------|-------------|
| Device ID | STM32 unique ID | ESP32-S3 eFuse ID |
| Authentication | Digital signature | ECDSA P-256 signature |
| Secure Boot | Yes | Yes (RSA-PSS) |
| Flash Encryption | Yes | Yes (AES-256-XTS) |
| Certificate | X.509 | esp_secure_cert |
| Clone Detection | On boot | On boot + runtime |

## Recommended Implementation Strategy

1. **Phase 1 (Development):**
   - Implement hardware authentication WITHOUT secure boot
   - Test on multiple devices
   - Log warnings only

2. **Phase 2 (Testing):**
   - Enable secure boot on test devices
   - Verify OTA updates work
   - Test key revocation

3. **Phase 3 (Production):**
   - Enable full security stack
   - Provision all production devices
   - Hard-fail on clone detection

## Support & Documentation

- ESP32-S3 Security Features: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/index.html
- Secure Boot Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html
- Flash Encryption: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/flash-encryption.html
- eFuse Manager: `espefuse.py --help`
