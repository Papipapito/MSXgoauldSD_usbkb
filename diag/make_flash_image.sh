#!/usr/bin/env bash
# =============================================================================
# make_flash_image.sh — Build a combined SPI-flash image for MSX Goa'uld
#                       (Tang Nano 20K / Gowin GW2AR-18)
# =============================================================================
#
# FLASH LAYOUT (Goa'uld external SPI-flash, 32 MB / 256 Mbit):
#
#   Offset 0x000000  Gowin FPGA bitstream (binary .bin)
#   Offset 0x200000  MSX BIOS pack (goauld_rom_int.bin, 512 KB)
#
# This layout is the standard Goa'uld layout: the Gowin configuration
# engine reads the bitstream starting at address 0, and the MSX core
# expects its BIOS data at SPI-flash offset 0x200000 (2 MiB mark).
#
# IMPORTANT — two ways to flash the board:
#
#   Method 1 (Gowin Programmer, RECOMMENDED):
#     Flash the .fs (ASCII bitstream) and the BIOS pack SEPARATELY:
#       a) Load the .fs into "External Flash" / "embFlash" at address 0x000000
#          (Gowin Programmer handles the .fs → binary conversion internally).
#       b) Load goauld_rom_int.bin into "External Flash" at address 0x200000.
#     This is the safest approach: no manual conversion, Gowin handles framing.
#
#   Method 2 (openFPGALoader or Gowin Programmer with raw binary):
#     Write this combined .bin image to the SPI flash at address 0x000000.
#       openFPGALoader -b tangnano20k --external-flash MSXgoauld_diag_flash.bin
#     or load it at offset 0 in Gowin Programmer's "External Flash" mode.
#
# DO NOT commit this output image or goauld_rom_int.bin to git:
#   - MSXgoauld_diag_flash.bin embeds the copyrighted MSX BIOS pack.
#   - goauld_rom_int.bin is a copyrighted MSX BIOS pack.
#   Distribute both as a private/local build artefact only.
#   The .fs bitstream and the .bin are excluded by .gitignore.
#   Offer the combined image and the BIOS pack via a private GitHub Release
#   with appropriate access restrictions, not as committed files.
#
# =============================================================================

set -euo pipefail

# --------------- Configurable paths and offsets ---------------

# Binary FPGA bitstream (produced by Gowin build: impl/pnr/project.bin)
# Pass as first argument or set here.
BITSTREAM_BIN="${1:-}"

# BIOS pack — MSX system ROMs, copyrighted, NOT to be committed
BIOS_PACK="${2:-$(dirname "$0")/../production/goauld_rom_int.bin}"

# Output combined flash image
OUTPUT="${3:-$(dirname "$0")/../MSXgoauld_diag_flash.bin}"

# Flash offsets
BIOS_OFFSET_HEX="0x200000"   # 2 MiB — Goa'uld standard BIOS location
BIOS_OFFSET_DEC=2097152      # $(printf '%d' $BIOS_OFFSET_HEX) — dd uses decimal

# Target flash total size (optional trim/pad — leave 0 to skip)
# 32 MiB flash: FLASH_SIZE=33554432
FLASH_SIZE=0

# --------------- Argument validation ---------------

usage() {
    echo "Usage: $0 <bitstream.bin> [bios_pack.bin] [output.bin]"
    echo ""
    echo "  bitstream.bin   Gowin binary bitstream (impl/pnr/project.bin)"
    echo "  bios_pack.bin   MSX BIOS pack (default: ../production/goauld_rom_int.bin)"
    echo "  output.bin      Output flash image  (default: ../MSXgoauld_diag_flash.bin)"
    exit 1
}

if [[ -z "$BITSTREAM_BIN" ]]; then
    echo "ERROR: bitstream.bin not specified." >&2
    usage
fi

if [[ ! -f "$BITSTREAM_BIN" ]]; then
    echo "ERROR: Bitstream not found: $BITSTREAM_BIN" >&2
    exit 1
fi

if [[ ! -f "$BIOS_PACK" ]]; then
    echo "ERROR: BIOS pack not found: $BIOS_PACK" >&2
    exit 1
fi

# --------------- Build image ---------------

BITSTREAM_SIZE=$(wc -c < "$BITSTREAM_BIN")
BIOS_SIZE=$(wc -c < "$BIOS_PACK")

echo "=== MSX Goa'uld flash image builder ==="
echo "  Bitstream : $BITSTREAM_BIN ($BITSTREAM_SIZE bytes)"
echo "  BIOS pack : $BIOS_PACK ($BIOS_SIZE bytes)"
echo "  BIOS offset: $BIOS_OFFSET_HEX ($BIOS_OFFSET_DEC decimal)"
echo "  Output    : $OUTPUT"
echo ""

# Sanity: bitstream must fit before the BIOS offset
if [[ $BITSTREAM_SIZE -ge $BIOS_OFFSET_DEC ]]; then
    echo "ERROR: Bitstream ($BITSTREAM_SIZE B) would overlap BIOS area at offset $BIOS_OFFSET_HEX" >&2
    exit 1
fi

# Remove stale output
rm -f "$OUTPUT"

# Step 1: Write bitstream at offset 0 into the image
# Use dd rather than cp so the output is always created with rw permissions
dd if="$BITSTREAM_BIN" of="$OUTPUT" bs=65536 2>/dev/null
chmod 644 "$OUTPUT"

# Step 2: Pad with 0xFF from end of bitstream to the BIOS offset.
# Flash erased state is 0xFF, so this matches physical flash default.
GAP=$(( BIOS_OFFSET_DEC - BITSTREAM_SIZE ))
echo "  Gap between bitstream end and BIOS offset: $GAP bytes (0xFF fill)"

# Write GAP bytes of 0xFF using dd + /dev/zero + tr trick (portable, no Python needed)
# We use printf to avoid spawning a separate interpreter
dd if=/dev/zero bs=1 count="$GAP" 2>/dev/null | tr '\000' '\377' >> "$OUTPUT"

# Step 3: Append BIOS pack at offset BIOS_OFFSET_DEC
cat "$BIOS_PACK" >> "$OUTPUT"

FINAL_SIZE=$(wc -c < "$OUTPUT")
echo ""
echo "=== Output flash image ==="
echo "  File  : $OUTPUT"
echo "  Size  : $FINAL_SIZE bytes ($(( FINAL_SIZE / 1024 )) KiB)"
echo ""
echo "Layout:"
printf "  0x%08X - 0x%08X  FPGA bitstream     (%d bytes)\n" \
    0 $(( BITSTREAM_SIZE - 1 )) "$BITSTREAM_SIZE"
printf "  0x%08X - 0x%08X  0xFF gap            (%d bytes)\n" \
    "$BITSTREAM_SIZE" $(( BIOS_OFFSET_DEC - 1 )) "$GAP"
printf "  0x%08X - 0x%08X  MSX BIOS pack      (%d bytes)\n" \
    "$BIOS_OFFSET_DEC" $(( BIOS_OFFSET_DEC + BIOS_SIZE - 1 )) "$BIOS_SIZE"

echo ""
echo "Flashing (openFPGALoader):"
echo "  openFPGALoader -b tangnano20k --external-flash \"$OUTPUT\""
echo ""
echo "DONE."
