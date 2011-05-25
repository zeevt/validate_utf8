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

#include <stdio.h>
#include <string.h>

#define CHECK_N_BYTES(n) \
  do {                                          \
    for (i = 0; i < n; i++) {                   \
      if ((c = getc_unlocked(ifile)) == EOF)    \
        goto out;                               \
      if (c < 0x80 || c >= 0xC0) {              \
        valid_utf8 = 0;                         \
        goto out;                               \
      }                                         \
    }                                           \
  } while(0)

int main(int argc, char* argv[])
{
  FILE *ifile;
  int all_7bit = 1, valid_utf8 = 1, c, i;
  if (argc < 2) {
    fprintf(stderr, "Usage: one file name argument or '-' for stdin.\n");
    return 1;
  }
  if (strcmp("-", argv[1]) == 0) {
    ifile = stdin;
  } else {
    if (!(ifile = fopen(argv[1], "rb"))) {
      fprintf(stderr, "Could not open file %s for input.\n", argv[1]);
      return 1;
    }
  }
  for (;;) {
    if ((c = getc_unlocked(ifile)) == EOF)
      goto out;
    if (c < 0x80)
      continue;
    all_7bit = 0;
    if (c < 0xC0) {
      valid_utf8 = 0;
      goto out;
    } else if (c <= 0xDF) {
      CHECK_N_BYTES(1);
    } else if (c <= 0xEF) {
      CHECK_N_BYTES(2);
    } else if (c <= 0xF7) {
      CHECK_N_BYTES(3);
    } else {
      /*
       * Four byte UTF-8 sequences allows to encode U+10FFFF.
       * Higher code points are not allowed in Unicode.
       */
      valid_utf8 = 0;
      goto out;
    }
  }
out:
  if (all_7bit)
    printf("ASCII\n");
  else if (valid_utf8)
    printf("UTF-8\n");
  else
    printf("Probably Latin-1\n");
  return 0;
}
