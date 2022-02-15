/*
// Benjamin Decre
// bed1@umbc.edu
//
// This program emulates free() and malloc().
//
// Sources:
// https://stackoverflow.com/questions/3649026/how-to-display-hexadecimal-numbers-in-c
// https://www.geeksforgeeks.org/program-next-fit-algorithm-memory-management/
// https://stackoverflow.com/questions/47107311/sending-signal-from-child-to-parent
// https://stackoverflow.com/questions/8011700/how-do-i-extract-specific-n-bits-of-a-32-bit-unsigned-integer-in-c
// https://stackoverflow.com/questions/1289251/converting-a-uint16-value-into-a-uint8-array2
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

pthread_mutex_t lock;

uint8_t memoryArray[640];
int freeFlag[10];
int SEED;
uint8_t LEFTBITS;
uint8_t RIGHTBITS;

void my_malloc_stats(void);

void *my_malloc(size_t size);

void my_free(void *ptr);

extern void hw4_test(void);

void sighandler(int sig);

int main(int argc, char *argv[]){
  
  int i;
  
  if(argc != 2){
    srand(0);
  }
  else{
    srand(strtoul(argv[1], NULL, 0));

    if(atoi(argv[1]) < 1){
      fprintf(stdout, "Invalid argument passed.\n");
      exit(1);
    }
  }
  
  for(i = 0; i < 640; i++){
    if(i < 10){
      freeFlag[i] = 1;
    }
    memoryArray[i] = 0;
  }
  
  hw4_test();
  
  return 0;
}

/*
 * This function prints out stats about the memory.
 * It outputs to the screen the stats
 */
void my_malloc_stats(void){

  pthread_mutex_lock(&lock);

  int i;
  int j;

  printf("Memory contents:\n");
  
  for(i = 0; i < 640; i+=64){
    for(j = 0; j < 64; j++){
      if(memoryArray[i + j] < 32 || memoryArray[i + j] > 126)
	printf(".");
      else
	printf("%c", memoryArray[i + j]);
    }
    printf("\n");
  }

  printf("Memory allocation table:\n");

  int k = 0;
  int found = 0;

  for(i = 0; i < 640; i+=64){
    if(freeFlag[k] == 1)
      printf("f:---- ");
    else{
      for(j = 0; j < 64; j++){
	if((memoryArray[i + j] != memoryArray[i + j + 1] && memoryArray[i + j + 1] != memoryArray[i + j + 2])){
	  printf("R:%02x%02x ", memoryArray[i+j+1], memoryArray[i+j+2]);
	  
	  int k;
	  for(k = found; k > 0; k--){
	    printf("R:---- ");
	  }

	  found = 0;
	  break;
	}
	else if(j == 63)
	  found++;
      }
    }
    k++;
  }

  printf("\n");

  pthread_mutex_unlock(&lock);
}

/*
 * This function allocates space in memory.
 * Inputs: the amount of bytes requested.
 * Outputs: a pointer to the memory.
 */
void *my_malloc(size_t size){

  pthread_mutex_lock(&lock);
  int i;
  int j;
  uint8_t *ptr = NULL;

  // Parse SEED for canary
  SEED = rand();
  uint16_t lowerBits;
  unsigned mask = (1 << 16) - 1;
  lowerBits = mask & SEED;

  uint8_t LEFTBITS = lowerBits & 0xff;
  uint8_t RIGHTBITS = (lowerBits >> 8);
  
  for(i = 639; i >= 0; i-=64){
    for(j = 0; j < 64; j++){
      
      if(memoryArray[i-j] != 0 && ((639 - (i + j + 1) + 2) > size)){
	ptr = &memoryArray[i+1];

	memoryArray[i + size + 1] = LEFTBITS;
	memoryArray[i + size + 2] = RIGHTBITS;	

	if(i == 575){
	  freeFlag[9] = 0;
	}
	else if(i == 511){
	  freeFlag[8] = 0;
	  if(size > 62)
	    freeFlag[9] = 0;
	}
	else if(i == 447){
	  freeFlag[7] = 0;
	  if(size > 62)
	    freeFlag[8] = 0;
	  if(size > 126)
	    freeFlag[9] = 0;
	}
	else if(i == 383){
	  freeFlag[6] = 0;
	  if(size > 62)
	    freeFlag[7] = 0;
	  if(size > 126)
	    freeFlag[8] = 0;
	  if(size > 190)
	    freeFlag[9] = 0;
	}
	else if(i == 319){
	  freeFlag[5] = 0;
	  if(size > 62)
	    freeFlag[6] = 0;
	  if(size > 126)
	    freeFlag[7] = 0;
	  if(size > 190)
	    freeFlag[8] = 0;
	  if(size > 254)
	    freeFlag[9] = 0;
	}
	else if(i == 255){
	  freeFlag[4] = 0;
	  if(size > 62)
	    freeFlag[5] = 0;
	  if(size > 126)
	    freeFlag[6] = 0;
	  if(size > 190)
	    freeFlag[7] = 0;
	  if(size > 254)
	    freeFlag[8] = 0;
	  if(size > 318)
	    freeFlag[9] = 0;
	}
	else if(i == 191){
	  freeFlag[3] = 0;
	  if(size > 62)
	    freeFlag[4] = 0;
	  if(size > 126)
	    freeFlag[5] = 0;
	  if(size > 190)
	    freeFlag[6] = 0;
	  if(size > 254)
	    freeFlag[7] = 0;
	  if(size > 318)
	    freeFlag[8] = 0;
	  if(size > 382)
	    freeFlag[9] = 0;
	}
	else if(i == 127){
	  freeFlag[2] = 0;
	  if(size > 62)
	    freeFlag[3] = 0;
	  if(size > 126)
	    freeFlag[4] = 0;
	  if(size > 190)
	    freeFlag[5] = 0;
	  if(size > 254)
	    freeFlag[6] = 0;
	  if(size > 318)
	    freeFlag[7] = 0;
	  if(size > 382)
	    freeFlag[8] = 0;
	  if(size > 446)
	    freeFlag[9] = 0;
	}
	else if(i == 63){
	  freeFlag[1] = 0;
	  if(size > 62)
	    freeFlag[2] = 0;
	  if(size > 126)
	    freeFlag[3] = 0;
	  if(size > 190)
	    freeFlag[4] = 0;
	  if(size > 254)
	    freeFlag[5] = 0;
	  if(size > 318)
	    freeFlag[6] = 0;
	  if(size > 382)
	    freeFlag[7] = 0;
	  if(size > 446)
	    freeFlag[8] = 0;
	  if(size > 510)
	    freeFlag[9] = 0;
	}

	pthread_mutex_unlock(&lock);
	return ptr;
      }

      if(memoryArray[0] == 0 && ptr == NULL){
	ptr = &memoryArray[0];

	memoryArray[size] = LEFTBITS;
	memoryArray[size + 1] = RIGHTBITS;

	freeFlag[0] = 0;
	if(size > 62)
	  freeFlag[1] = 0;
	if(size > 126)
	  freeFlag[2] = 0;
	if(size > 190)
	  freeFlag[3] = 0;
	if(size > 254)
	  freeFlag[4] = 0;
	if(size > 318)
	  freeFlag[5] = 0;
	if(size > 382)
	  freeFlag[6] = 0;
	if(size > 446)
	  freeFlag[7] = 0;
	if(size > 510)
	  freeFlag[8] = 0;
	if(size > 572)
	  freeFlag[9] = 0;

	pthread_mutex_unlock(&lock);
	return ptr;
      }      
    }
  }
  
  errno = ENOMEM;
  pthread_mutex_unlock(&lock);
  return ptr;
}

/*
 * This function frees memory
 * Inputs: pointer to memory
 */
void my_free(void *ptr){
  
  pthread_mutex_lock(&lock);

  int i;
  int j;

  int check = 0;
  
  for(i = 0; i < 640; i++){
    if(&memoryArray[i] == ptr){
      
      check = 1;
      int checkArray[10] = {0};
      
      for(j = i; j < 640; j++){
	if(memoryArray[j] == memoryArray[j+1] && memoryArray[j] == memoryArray[j+2]){
	  if(j >= 0 && j < 64){
	    if(freeFlag[0] == 1 && checkArray[0] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[0] = 1;
	    freeFlag[0] = 1;
	  }
	  if(j >= 64 && j < 128){
	    if(freeFlag[1] == 1 && checkArray[1] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[1] = 1;
	    freeFlag[1] = 1;
	  }
	  if(j >= 128 && j < 192){
	    if(freeFlag[2] == 1 && checkArray[2] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[2] = 1;
	    freeFlag[2] = 1;
	  }
	  if(j >= 192 && j < 256){
	    if(freeFlag[3] == 1 && checkArray[3] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[3] = 1;
	    freeFlag[3] = 1;
	  }
	  if(j >= 256 && j < 320){
	    if(freeFlag[4] == 1 && checkArray[4] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[4] = 1;
	    freeFlag[4] = 1;
	  }
	  if(j >= 320 && j < 384){
	    if(freeFlag[5] == 1 && checkArray[5] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[5] = 1;
	    freeFlag[5] = 1;
	  }
	  if(j >= 384 && j < 448){
	    if(freeFlag[6] == 1 && checkArray[6] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[6] = 1;
	    freeFlag[6] = 1;
	  }
	  if(j >= 448 && j < 512){
	    if(freeFlag[7] == 1 && checkArray[7] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[7] = 1;
	    freeFlag[7] = 1;
	  }
	  if(j >= 512 && j < 576){
	    if(freeFlag[8] == 1 && checkArray[8] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[8] = 1;
	    freeFlag[8] = 1;
	  }
	  if(j >= 576 && j < 640){
	    if(freeFlag[9] == 1 && checkArray[9] == 0){
	      raise(SIGSEGV);
	      signal(SIGSEGV, sighandler);
	    }
	    checkArray[9] = 1;
	    freeFlag[9] = 1;
	  }
	}
	else
	  break;
	// end of inner for loop
      }
      break;
      // end of if statement
    }
    // end of outer for loop 
  }

  if(check == 0){
    signal(SIGSEGV, sighandler);
  }
  pthread_mutex_unlock(&lock);
}

/*
 * Signal handling function
 */
void sighandler(int sig){
  printf("Caught signal.\n");
  return;
}
