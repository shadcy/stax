import sys
import struct

def create_mbr(start_lba, size_sectors):
    mbr = bytearray(512)
    # Partition 1 entry at offset 0x1BE
    # Status (0x00), CHS start (3 bytes), Type (0x06 FAT16), CHS end (3 bytes)
    # LBA start (4 bytes), LBA size (4 bytes)
    struct.pack_into('<B 3s B 3s I I', mbr, 0x1BE, 
                     0x00, b'\x00\x00\x00', 0x06, b'\x00\x00\x00', start_lba, size_sectors)
    # Signature
    mbr[510] = 0x55
    mbr[511] = 0xAA
    return mbr

if __name__ == '__main__':
    start_lba = int(sys.argv[1]) if len(sys.argv) > 1 else 4099
    size_sectors = int(sys.argv[2]) if len(sys.argv) > 2 else 60000
    mbr = create_mbr(start_lba, size_sectors)
    with open('mbr.bin', 'wb') as f:
        f.write(mbr)
