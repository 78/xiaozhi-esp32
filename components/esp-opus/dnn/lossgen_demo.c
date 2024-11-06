#include <stdio.h>
#include <stdlib.h>
#include "lossgen.h"
int main(int argc, char **argv)
{
   LossGenState st;
   long num_packets;
   long i;
   float percent;
   if (argc != 3) {
      fprintf(stderr, "usage: %s <percent_loss> <nb packets>\n", argv[0]);
      return 1;
   }
   lossgen_init(&st);
   percent = atof(argv[1]);
   num_packets = atol(argv[2]);
   /*printf("loss: %f %d\n", percent, num_packets);*/
   for (i=0;i<num_packets;i++) {
      printf("%d\n", sample_loss(&st, percent*0.01f));
   }
   return 0;
}
