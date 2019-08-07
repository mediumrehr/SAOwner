#ifndef SAOWNER_H_INCLUDED
#define SAOWNER_H_INCLUDED

#include "asf.h"
#include <stdio.h>
#include <string.h>

extern void test(uint8_t *printStr, uint8_t strLen);

#define DATA_LENGTH 50
extern uint8_t read_buffer_in [DATA_LENGTH];
extern struct i2c_master_packet rd_packet_out;

#endif /* SAOWNER_H_INCLUDED */
