/* Benjamin Decre 
 * bed1@umbc.edu
 *
 * Test program to test sha3.c for errors.
 *
 * Sources:
 * http://man7.org/linux/man-pages/man2/mmap.2.html
 */

#include <stdio.h>
#include <sys/user.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

int main(void)
{
  int errors = 0;

  // TEST 0, TESTING IF THE MODULE IS LOADED
  FILE *modfd = popen("lsmod | grep -w \"sha3\"", "r");

  char str[16];
  if (fread (str, 1, sizeof(str), modfd) > 0)
    printf("Module is loaded.\n");
  else{
    printf("Module is not loaded, please load the module first.\n");
    exit(1);
  }
  
  // TEST 1, TESTING THE GIVEN EXAMPLE.
  // GOAL: match program output with given output
  int fd1 = open("/dev/sha3_data", O_RDWR);
  char input[8] = "CMSC 421";  
  char *map1 = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0); 
  memcpy(map1, input, 8);
  
  int fd2 = open("/dev/sha3_ctl", O_RDWR);
  char buf[1] = "8";
  
  if(!write(fd2, buf, 1)){
    printf("Error: attempting to write to sha3_ctl fails.\n");
    errors += 1;
  }

  printf("TEST 1 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 1: %d\n", errors);
  
  // TEST 2, WRITING NOTHING TO CTL
  // GOAL: Test to see if sha3_ctl gets altered after writing nothing.
  char input1[8] = "CMSC 421";  
  
  memcpy(map1, input1, 8);

  fd2 = open("/dev/sha3_ctl", O_RDWR);
  int zero = 0;
  buf[0] = (char) zero;

  if(!write(fd2, buf, 0)){
    errors += 1;
  }
  
  char outbuf[64] = {0};
  if(!read(fd2, outbuf, sizeof(outbuf))){
    errors += 1;
  };

  printf("TEST 2 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 2: %d\n", errors);

  // TEST 3, Trying to read more than a PAGE_SIZE
  // GOAL: Output array should be empty, thus nothing was read.
  int fd3 = open("/dev/sha3_data", O_RDWR);

  char lots[4098] = {0};
  
  if(read(fd3, lots, sizeof(lots)))
    errors += 1;

  int i;
  for(i = 0; i < 4098; i++){
    if(lots[i] != 0){
      printf("Error, read when it wasn't supposed to.\n");
      errors += 1;
      break;
    }
  }
  
  printf("TEST 3 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 3: %d\n", errors);

  // TEST 4, trying to hash more than 4096 bytes to sha3_ctl
  // GOAL: sha3_ctl isn't changing after an oversized write
  int fd4 = open("/dev/sha3_ctl", O_RDWR);

  int errBuf[1] = { 4098 };
  
  if(!write(fd4, errBuf, sizeof(errBuf))){
    printf("Error: writing too many bytes from data succeeds.\n");
    errors += 1;
  }

  printf("TEST 4 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 4: %d\n", errors);

  // TEST 5, trying to read too many bytes from sha3_ctl
  // GOAL: make sure it doesn't put too many bytes into output buffer
  int fd5 = open("/dev/sha3_ctl", O_RDWR);

  char outputBuf[80] = {0};

  if(!read(fd5, outputBuf, sizeof(outputBuf))){
    printf("Error: reading too many bytes from ctl fails.\n");
    errors += 1;
  }
  
  if(outputBuf[65] != 0){
    printf("Error: too many bytes read.\n");
    errors += 1;
  }
    
  printf("TEST 5 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 5: %d\n", errors);

  // TEST 6, reading nothing from CTL
  // GOAL: assure no bytes are returned when reading 0.
  int fd6 = open("/dev/sha3_ctl", O_RDWR);
  
  char outbufTest[64] = {0};

  if(read(fd6, outbufTest, 0) != 0){
    printf("Error: reading nothing actually reads something.\n");
    errors += 1;
  }

  printf("TEST 6 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 6: %d\n", errors);

  // TEST 7, allocating too little space for data
  // GOAL: assure this does not break the program.
  int fd7 = open("/dev/sha3_data", O_RDWR);
  char input7[8] = "CMSC 421";
  char *map2 = mmap(0, 2048, PROT_READ | PROT_WRITE, MAP_SHARED, fd7, 0); 
  memcpy(map2, input7, 8);

  printf("TEST 7 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 7: %d\n", errors);

  // TEST 8, attempting to write more than PAGE_SIZE bytes to data
  // GOAL: assure this does not break the program.
  int fd8 = open("/dev/sha3_data", O_RDWR);
  char input8[5000];
  for(i = 0; i < 5000; i++){
    input8[i] = 1;
  }

  char *map3 = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd8, 0);
  memcpy(map3, input8, 5000);

  printf("TEST 8 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 8: %d\n", errors);

  // TEST 9, zero out the memory map
  // GOAL: sha3_data has nothing in it
  int fd9 = open("/dev/sha3_data", O_RDWR);
  char zeroArr[4096] = {0};

  char *map4 = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd9, 0);
  memcpy(map4, zeroArr, sizeof(zeroArr));

  printf("TEST 9 PASSED WITHOUT FAILING.\n");
  printf("Errors after test 9: %d\n", errors);
  
  // Clean up

  printf("Testing complete.");
  
  munmap(map1, 8);
  munmap(map2, 8);
  munmap(map3, PAGE_SIZE);
  
  close(fd1);
  close(fd2);
  close(fd3);
  close(fd4);
  close(fd5);
  close(fd6);
  close(fd7);
  close(fd8);
  close(fd9);
  
  return 0;
}
