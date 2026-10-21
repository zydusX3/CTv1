#pragma once
#include <stddef.h>
#include <stdint.h>

// AES-128 single-block encrypt. in and out may be the same buffer.
void ctv1_aes128_encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

// AES-CMAC (RFC 4493), full 16-byte tag.
void ctv1_aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t mac[16]);
