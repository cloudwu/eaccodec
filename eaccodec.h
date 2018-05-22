#ifndef eac_codec_h
#define eac_codec_h

#include <stdint.h>

void eac_encode(const uint8_t alpha[16], uint8_t data[8]);
void eac_decode(const uint8_t data[8], uint8_t alpha[16]);

#endif
