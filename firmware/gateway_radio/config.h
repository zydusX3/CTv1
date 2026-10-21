#pragma once
// Home beacon radio settings. FARM_KEY and every LORA_* value must be identical to firmware/tag/config.h.
#define FARM_KEY {0x43, 0x54, 0x76, 0x31, 0x2d, 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x2d, 0x6d, 0x65, 0x21, 0x21}

#define LORA_FREQ_HZ 865062500
#define LORA_SF 9
#define LORA_BW_KHZ 125
#define LORA_CR 0
#define LORA_PREAMBLE 8
#define LORA_TX_DBM 14

#define ACK_DELAY_MS 50      // give the tag time to switch to receive after it finishes sending
#define MAX_TAGS 64          // tags remembered for duplicate detection and queued commands
#define STATUS_EVERY_MS 60000

// Where the line protocol goes (see lib/ctv1/src/ctv1_gwlink.h):
//   0 = UART1 (PB6/PB7) to the ESP32-S3, as in docs/03-hardware-home-beacon.md
//   1 = UART2, the USB programming port: plug the module into a PC and run tools/gw_monitor.py
#define LINK_ON_USB 1
#define LINK_BAUD 115200
