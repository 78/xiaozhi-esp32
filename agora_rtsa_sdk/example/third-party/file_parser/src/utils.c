#include <stdio.h>

long get_file_size(FILE *f)
{
  long file_size;

  /* Go to end of file */
  fseek(f, 0L, SEEK_END);

  /* Get the number of bytes */
  file_size = ftell(f);

  /* reset the file position indicator to 
    the beginning of the file */
  fseek(f, 0L, SEEK_SET);

  return file_size;
}