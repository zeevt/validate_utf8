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
 * Comile with: gcc -std=gnu89 -Wall -Wextra -pedantic -O2
 *  -g -o guess_charset guess_charset.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#if defined(__x86_64__)
#  define REGSIZE_TYPE          uint64_t
#  define MSB_MASK              0x8080808080808080UL
#  define bitscan_le(v)         (__builtin_ffsl(v))
#elif defined(__i386__)
#  define REGSIZE_TYPE          uint32_t
#  define MSB_MASK              0x80808080U
#  define bitscan_le(v)         (__builtin_ffs(v))
#else
#  error Only x86 and x86-64 are currently supported.
#endif
#define get_unaligned_reg(p)    (*(const REGSIZE_TYPE *)(p))
#define get_unaligned_16(p)     (*(const uint16_t *)(p))
#define PAGE_SHIFT              (12)
#define PAGE_SIZE               (1 << PAGE_SHIFT)

static uint8_t *guard_start, *guard_end;
static volatile int all_7bit;

static void __attribute__((noreturn)) is_valid_utf8(const uint8_t *curr)
{
  static const void * const jump_table[] = {
    &&bad, &&bad, &&bad, &&bad,
    &&bad, &&bad, &&bad, &&bad,
    &&one_byte, &&one_byte, &&one_byte, &&one_byte,
    &&two_bytes, &&two_bytes, &&three_bytes, &&bad
  };
  int c;
  REGSIZE_TYPE v;
  /* skip sizeof(v) bytes at a time, provided they all have the MSB off */
  for (;;) {
    v = get_unaligned_reg(curr) & MSB_MASK;
    if (likely(v == 0)) {
      curr += sizeof(v);
    } else {
      curr += (bitscan_le(v) >> 3) - 1;
      c = *curr++;
      break;
    }
  }
  all_7bit = 0;
  goto *jump_table[(c >> 3) & 15];
three_bytes:
  c = *curr++;
  if (unlikely(c >> 6 != 2))
    goto bad;
two_bytes:
  if (unlikely((get_unaligned_16(curr) & 0xC0C0) != 0x8080))
    goto bad;
  curr += 2;
  c = *curr++;
  if (unlikely(c < 128))
    goto next_byte2;
  goto *jump_table[(c >> 3) & 15];
one_byte:
  c = *curr++;
  if (unlikely(c >> 6 != 2))
    goto bad;
  c = *curr++;
  if (unlikely(c < 128))
    goto next_byte2;
  goto *jump_table[(c >> 3) & 15];
next_byte2:
  c = *curr++;
  if (likely(c >= 128))
    goto utf8_payload;
  c = *curr++;
  if (likely(c >= 128))
    goto utf8_payload;
  for (;;) {
    v = get_unaligned_reg(curr) & MSB_MASK;
    if (likely(v == 0)) {
      curr += sizeof(v);
    } else {
      curr += (bitscan_le(v) >> 3) - 1;
      c = *curr++;
      break;
    }
  }
utf8_payload:
  goto *jump_table[(c >> 3) & 15];
bad:
  printf("Probably Latin-1\n");
  exit(0);
}

/* TODO: switch from stdio to write(2) calls in sig handler */
void handler(int cause, siginfo_t *info, void *uap)
{
  if (likely((uint8_t *)info->si_addr >= guard_start &&
             (uint8_t *)info->si_addr < guard_end)) {
    if (all_7bit)
      printf("ASCII\n");
    else
      printf("UTF-8\n");
    if (unlikely(mprotect(guard_start, guard_end - guard_start, PROT_READ | PROT_WRITE))) {
      perror("mprotect");
      exit(1);
    }
    exit(0);
  } else {
    fprintf(stderr, "Unexpected SIGSEGV @ %p\n", info->si_addr);
    exit(1);
  }
}

int main(int argc, char* argv[])
{
  struct stat st;
  size_t next_page;
  int fd;
  unsigned char *base;
  struct sigaction sa;
  sa.sa_sigaction = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  if (unlikely(sigaction(SIGSEGV, &sa, 0))) {
    perror("sigaction");
    return 1;
  }
  if (unlikely(argc < 2)) {
    fprintf(stderr, "Usage: one file name argument.\n");
    return 1;
  }
  if (unlikely((fd = open(argv[1], O_RDONLY)) == -1)) {
    perror("open");
    return 1;
  }
  if (unlikely(fstat(fd, &st))) {
    perror("fstat");
    return 1;
  }
  next_page = ((st.st_size + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT;
  if (unlikely((base = mmap(NULL, next_page + PAGE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)) {
    fprintf(stderr, "Could not mmap file %s for input.\n", argv[1]);
    return 1;
  }
  guard_start = base + next_page;
  guard_end = base + next_page + PAGE_SIZE;
  if (unlikely(mprotect(base + next_page, PAGE_SIZE, PROT_NONE))) {
    perror("mprotect");
    return 1;
  }
  all_7bit = 1;
  is_valid_utf8(base);
  fprintf(stderr, "Shouldn't reach here.\n");
  return 1;
}
