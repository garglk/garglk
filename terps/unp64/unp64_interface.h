//
//  unp64_interface.h
//
//  Created by Petter Sjölund on 2022-01-31.
//

#ifndef unp64_interface_h
#define unp64_interface_h

#include <stdint.h>

int unp64(uint8_t *compressed, size_t length, uint8_t *destination_buffer,
          size_t *final_length, char *settings[], int numsettings);

#endif /* unp64_interface_h */
