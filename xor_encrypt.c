/**
 *    file: xor_encrypt.c
 *    created at: 4 Aug 2023
 *
 *    usage:
 *      ./enc [input file] [output file] [KEY]      
 *      KEY is in range 0x00 to 0xFF
 *
 *
 *    compilation:
 *      cc -o enc -Wall -Wextra -DDEBUG xor_encrypt.c
 *
 *    use -D ESCAPE_HEAD
 *          if you don't want the magic byte of
 *          your files to be affected
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE 512
#define HEAD_LEN 10


#define xor_alg(j, x) buff[j] ^= x

#ifdef DEBUG
#define logf(format, ...) \
  do { if (DEBUG) printf(format, __VA_ARGS__); } while(0)
#else
#define logf(format, ...) ((void)0)
#endif

#define HR_format(bytes)                            \
  ((bytes) >= (1 << 30) ? "GB" :                    \
  ((bytes) >= (1 << 20) ? "MB" :                    \
  ((bytes) >= (1 << 10) ? "KB" : "B")))

#define HR_size(bytes)                                               \
  ((double)(bytes) / ((bytes) >= (1 << 30) ? (1ULL << 30)   :        \
                     ((bytes) >= (1 << 20) ? (1ULL << 20)   :        \
                     ((bytes) >= (1 << 10) ? (1ULL << 10) : 1))))



int enc(FILE *in, FILE *out, char xor)
{
  char buff[BUFF_SIZE];
  size_t i, j;
  int write_l = 0, read_l = 0;


#ifdef ESCAPE_HEAD
  // copy first HEAD_LEN bytes of the input file
  i = fread(buff, 1, HEAD_LEN, in);
  read_l += i;
  write_l += fwrite(buff, 1, i, out);
#endif


  // copy and XOR
  while ((i = fread(buff, 1, BUFF_SIZE, in)) > 0)
    {
      for(j=0; j<i; ++j)
        xor_alg(j, xor);

      read_l += i;
      write_l += fwrite(buff, 1, i, out);
    }

  logf("%d records in\n", read_l);
  return write_l;
}


int main(int argc, char **argv)
{
  unsigned int xor;
  FILE *file_in, *file_out;

  
  if(argc<4)
    {
      fprintf(stderr, "Not Enough Input Arguments\n");
      fprintf(stderr, "Usage: ./enc [input file] [output file] 0x[00 to ff]\n");
      return 1;
    }

  if(strcmp(argv[1], argv[2]) == 0)
    {
      fprintf(stderr, "input file and output file are the same.\n");
      return 1;
    }

  // read xor byte
  sscanf(argv[3], "%x", &xor);
  logf("config: in=%s, out=%s, XOR byte=0x%x\n",
         argv[1], argv[2], xor);


  
  // open files
  file_in  = fopen(argv[1], "r");
  file_out = fopen(argv[2], "w");
  
  
  if(file_in == NULL)
    {
      printf("could not open input file.\n");
      fclose(file_in);
      return 1;
    }
  else if(file_out == NULL)
    {
      printf("could not open output file.\n");
      fclose(file_out);
      return 1;
    }
  else
    {
      int _len = enc(file_in, file_out, (char)xor);

      printf("%d bytes (%.1f %s) copied.\n",
             _len, HR_size(_len), HR_format(_len));

      fclose(file_in);
      fclose(file_out);
    }

  return 0;
}
