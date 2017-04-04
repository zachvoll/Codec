/*
 * name: Zachary Vollen, Yadu Kiran
 * x500: voll0139, kiran013
 * CSELabs machine: CSEL-KH1200-14
 */
 
 /*
  * Login: voll0139, kiran013
  * Date: 10/28/2015
  * ID: voll0139(4381309),kiran013(5183492(
  */

#ifndef __CODEC_H__
#define __CODEC_H__

#include <stdint.h>
#include <stddef.h>

/* Returns 1 if 'val' is a valid char under the encoding scheme */
int is_valid_char(uint8_t val);

/* encode 3x 8-bit binary bytes as 4x '6-bit' characters */
size_t encode_block(uint8_t *inp, uint8_t *out, int len);

/* decode 4x '6-bit' characters into 3x 8-bit binary bytes */
size_t decode_block(uint8_t *inp, uint8_t *out);

#endif /* __CODEC_H__ */
