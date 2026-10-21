#pragma once
// u-blox UBX messages for the MAX-M10S (protocol 34+). Sent once after power-up; saved in the
// battery-backed RAM (V_BCKP), so they survive the GPS being switched off between fixes.
#include <stddef.h>
#include <stdint.h>

// Wraps a payload in a UBX frame (sync chars, class, id, length, Fletcher checksum). Returns the frame length.
size_t ctv1_ubx_frame(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *out, size_t out_max);

// UBX-CFG-VALSET (RAM + BBR layers) that turns off every NMEA sentence except GGA and RMC on UART1.
// Less to transmit and parse; the M10 sends GSV/GSA/GLL/VTG by default (~600 bytes/s).
size_t ctv1_ubx_nmea_only_gga_rmc(uint8_t *out, size_t out_max);
