#!/usr/bin/env python3
"""
uFlake Device Provisioning Tool
================================

This script provisions uFlake devices during manufacturing by:
1. Generating unique device identity
2. Signing identity with your private key
3. Burning identity to eFuse (PERMANENT!)

SECURITY WARNING:
- Run this on an OFFLINE, SECURE computer
- Keep device_identity_private_key.pem SECRET and BACKED UP
- eFuse burns are PERMANENT and CANNOT be undone
- Test on sacrificial devices first!

Usage:
    python provision_device.py --serial SN123456 --hwver 1 --rev 1
"""

import argparse
import hashlib
import time
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.backends import default_backend

def load_private_key(key_file="device_identity_private_key.pem"):
    """Load the ECDSA private key"""
    try:
        with open(key_file, 'rb') as f:
            private_key = serialization.load_pem_private_key(
                f.read(),
                password=None,
                backend=default_backend()
            )
        print(f"✓ Loaded private key from {key_file}")
        return private_key
    except Exception as e:
        print(f"✗ Failed to load private key: {e}")
        return None

def generate_device_identity(serial, hw_version, board_revision, device_id):
    """Generate device identity structure"""
    identity = {
        'device_id': device_id,  # 16 bytes - from ESP32 eFuse
        'serial_number': serial.ljust(32, '\x00')[:32],  # 32 bytes - padded
        'hw_version': hw_version,  # 1 byte
        'board_revision': board_revision,  # 1 byte
        'manufacture_date': int(time.time()),  # 4 bytes - Unix timestamp
    }
    return identity

def calculate_identity_hash(identity):
    """Calculate SHA-256 hash of device identity"""
    hasher = hashlib.sha256()
    
    # Hash in the same order as C code
    hasher.update(bytes.fromhex(identity['device_id']))
    hasher.update(identity['serial_number'].encode('utf-8'))
    hasher.update(bytes([identity['hw_version']]))
    hasher.update(bytes([identity['board_revision']]))
    hasher.update(identity['manufacture_date'].to_bytes(4, 'little'))
    
    return hasher.digest()

def sign_identity(private_key, identity_hash):
    """Sign the device identity hash with ECDSA"""
    signature = private_key.sign(
        identity_hash,
        ec.ECDSA(hashes.SHA256())
    )
    return signature

def generate_c_header(identity, signature):
    """Generate C header file for flashing to device"""
    
    device_id_hex = ', '.join([f'0x{b:02x}' for b in bytes.fromhex(identity['device_id'])])
    serial_hex = ', '.join([f'0x{ord(c):02x}' for c in identity['serial_number']])
    sig_hex = ', '.join([f'0x{b:02x}' for b in signature])
    
    header = f"""// AUTO-GENERATED - DO NOT EDIT
// Device: {identity['serial_number'].strip()}
// Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}

#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include "hw_auth.h"

const hw_identity_t DEVICE_IDENTITY = {{
    .device_id = {{{device_id_hex}}},
    .serial_number = {{{serial_hex}}},
    .hw_version = {identity['hw_version']},
    .board_revision = {identity['board_revision']},
    .manufacture_date = {identity['manufacture_date']},
    .signature = {{{sig_hex}}}
}};

#endif // DEVICE_IDENTITY_H
"""
    return header

def main():
    parser = argparse.ArgumentParser(description='Provision uFlake device')
    parser.add_argument('--serial', required=True, help='Device serial number (e.g., UFH-001234)')
    parser.add_argument('--hwver', type=int, default=1, help='Hardware version')
    parser.add_argument('--rev', type=int, default=1, help='Board revision')
    parser.add_argument('--device-id', required=True, help='Device unique ID (32 hex chars from eFuse)')
    parser.add_argument('--key', default='device_identity_private_key.pem', help='Private key file')
    parser.add_argument('--output', default='device_identity.h', help='Output header file')
    parser.add_argument('--yes', action='store_true', help='Skip confirmation prompt')
    
    args = parser.parse_args()
    
    # Validate device ID
    if len(args.device_id) != 32:
        print(f"✗ Device ID must be 32 hex characters (got {len(args.device_id)})")
        return 1
    
    print("\n" + "="*60)
    print("uFlake Device Provisioning Tool")
    print("="*60)
    
    # Load private key
    private_key = load_private_key(args.key)
    if not private_key:
        return 1
    
    # Generate identity
    print(f"\nGenerating identity for device:")
    print(f"  Serial Number:   {args.serial}")
    print(f"  Hardware Ver:    {args.hwver}")
    print(f"  Board Revision:  {args.rev}")
    print(f"  Device ID:       {args.device_id}")
    
    identity = generate_device_identity(args.serial, args.hwver, args.rev, args.device_id)
    
    # Calculate hash
    identity_hash = calculate_identity_hash(identity)
    print(f"  Identity Hash:   {identity_hash.hex()}")
    
    # Sign
    print("\n📝 Signing identity...")
    signature = sign_identity(private_key, identity_hash)
    print(f"✓ Signature generated ({len(signature)} bytes)")
    
    # Generate header
    header_content = generate_c_header(identity, signature)
    
    # Show preview
    print("\n" + "-"*60)
    print("Preview of generated header:")
    print("-"*60)
    print(header_content[:500] + "...")
    
    # Confirmation
    if not args.yes:
        print("\n⚠️  WARNING: This will generate a provisioning file.")
        print("⚠️  Burning this to eFuse is PERMANENT and CANNOT be undone!")
        response = input("\nContinue? (yes/no): ")
        if response.lower() != 'yes':
            print("Aborted.")
            return 0
    
    # Write header file
    with open(args.output, 'w') as f:
        f.write(header_content)
    
    print(f"\n✓ Device identity written to: {args.output}")
    print("\nNext steps:")
    print(f"  1. Flash this identity to device {args.serial}")
    print(f"  2. Run provisioning firmware to burn to eFuse")
    print(f"  3. BACKUP the identity file for your records")
    print(f"  4. Verify device authentication works")
    print("\n⚠️  REMEMBER: eFuse burns are PERMANENT!\n")
    
    return 0

if __name__ == '__main__':
    exit(main())
