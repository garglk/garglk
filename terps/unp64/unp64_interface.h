/* Unp64 ScummVM version C interface written by Petter Sjölund */

#ifndef UNP64_INTERFACE_H
#define UNP64_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

extern int unp64(uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t *finalLength, const char *settings);

#ifdef __cplusplus
}
#endif

#endif 
