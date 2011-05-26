/*
Copyright 2011 Zeev Tarantov. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY Zeev Tarantov ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Zeev Tarantov.
*/
/*
 * Comile with: gcc -std=c89 -D_POSIX_C_SOURCE=1 -Wall -Wextra -pedantic -O2
 *  -g -o guess_charset guess_charset.c
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define MIN(a,b)        (((a)<(b))?(a):(b))

#define get_unaligned64(x)      (*(const uint64_t *)(x))

enum encoding {
  ASCII,
  UTF8,
  UNKNOWN
};

#define BYTE_LOOP_UNROLL        4
#define REGSIZE_BYTES           8

#define NEXT_BYTE               \
  c = *curr++;

#define IS_ASCII                \
  if (c >= 0x80)                \
    goto utf8_payload;

#define IS_PAYLOAD                              \
  if (unlikely((c < 0x80) || (c >= 0xC0)))      \
    return UNKNOWN;

static enum encoding is_valid_utf8(
  const unsigned char *curr,
  const unsigned char * const end)
{
  int all_7bit = 1, c, i;
  uint64_t v;
  for (;;) {
byte_search:
    /* skip one byte at a time, if it has MSB off */
    if (likely(curr <= end - BYTE_LOOP_UNROLL)) {
      NEXT_BYTE IS_ASCII
      NEXT_BYTE IS_ASCII
      NEXT_BYTE IS_ASCII
      NEXT_BYTE IS_ASCII
    } else {
      for (i = BYTE_LOOP_UNROLL; i; i--) {
        if (unlikely(curr == end))
          goto out;
        NEXT_BYTE IS_ASCII
      }
    }
    /* skip REGSIZE_BYTES bytes at a time, provided they all have the MSB off */
    while (likely(curr <= end - REGSIZE_BYTES)) {
      v = get_unaligned64(curr) & 0x8080808080808080UL;
      if (v == 0) {
        curr += REGSIZE_BYTES;
      } else {
        curr += (__builtin_ffsl(v) >> 3) - 1;
        c = *curr++;
        goto utf8_payload;
      }
    }
    goto byte_search;
utf8_payload:
    all_7bit = 0;
    if (unlikely(c < 0xC0))
      return UNKNOWN;
    else if (c <= 0xDF)
      goto check_1_byte;
    else if (c <= 0xEF)
      goto check_2_bytes;
    else if (likely(c <= 0xF7))
      goto check_3_bytes;
    else
      return UNKNOWN;
check_3_bytes:
    if (unlikely(curr == end))
      return UNKNOWN;
    NEXT_BYTE IS_PAYLOAD
check_2_bytes:
    if (unlikely(curr == end))
      return UNKNOWN;
    NEXT_BYTE IS_PAYLOAD
check_1_byte:
    if (unlikely(curr == end))
      return UNKNOWN;
    NEXT_BYTE IS_PAYLOAD
  }
out:
  return all_7bit ? ASCII : UTF8;
}

int main(int argc, char* argv[])
{
  struct stat st;
  int fd;
  unsigned char *base;
  enum encoding result;
  if (argc < 2) {
    fprintf(stderr, "Usage: one file name argument.\n");
    return 1;
  }
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Could not open file %s for input.\n", argv[1]);
    return 1;
  }
  if (fstat(fd, &st)) {
    fprintf(stderr, "Could not stat file %s for input.\n", argv[1]);
    return 1;
  }
  if ((base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
    fprintf(stderr, "Could not mmap file %s for input.\n", argv[1]);
    return 1;
  }
  result = is_valid_utf8(base, base + st.st_size);
  munmap(base, st.st_size);
  if (result == ASCII)
    printf("ASCII\n");
  else if (result == UTF8)
    printf("UTF-8\n");
  else
    printf("Probably Latin-1\n");
  return 0;
}
