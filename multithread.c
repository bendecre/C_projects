/*
//
// Benjamin Decre
// bed1@umbc.edu
//
// Delivery program to send out threads that wait a specific amount of time after receiving an order. 
//
// Sources:
// https://stackoverflow.com/questions/21180248/fgets-to-read-line-by-line-in-files
// https://www.geeksforgeeks.org/structures-c/
// https://linux.die.net/man/3/atoi
// https://fresh2refresh.com/c-programming/c-array-of-structures/
// https://stackoverflow.com/questions/1031872/extract-integer-from-char-buffer
// https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html
// http://man7.org/linux/man-pages/man3/usleep.3.html
// http://www.cplusplus.com/forum/unices/23581/
// https://stackoverflow.com/questions/23178741/access-members-of-a-structure-from-void-pointer-inside-a-structure
// http://www.csc.villanova.edu/~mdamian/threads/posixsem.html
//
// Part 6:
// The restaurant thread is the producer since it produces the orders.
// The delivery threads are the consumer since it consumes the orders.
// The data item is the orders.
//
// In the main thread, when displaying the total amount of orders, if a driver has incremented the number of deliveries they've completed as the total amount of deliveries is summed, there could be a race condition between these two numbers, and total number displayed will be one less than the actual amount of total orders completed.
*/
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

pthread_mutex_t lock;

sem_t NUM_ORDERS;
int ORDER_NUM;
int TOTAL;
int MAX_TIME;

FILE* INFILE;

// Coordinate structure to pass multiple values to other functions.
struct coords{
  int x;
  int y;
};

// Driver structure to send driver information to the deliver thread
struct driver{
  char name[100];
  int count;
  int x;
  int y;
  char status[5];
};

struct coords ORDERS[100];
struct coords REST_COORDS[10];
struct coords PEOPLE_COORDS[10];

// Driver function to delivery the food to the customer
// Takes in a pointer to a struct of type driver, defined above.
void *deliver(void *ptr){
  
  while(1){
    sem_wait(&NUM_ORDERS);
    
    int myorder = TOTAL;
    TOTAL += 1;
    ORDER_NUM--;

    // Travel to restuarant
    int tempx = REST_COORDS[ORDERS[myorder].x].x;
    int tempy = REST_COORDS[ORDERS[myorder].x].y;
    
    int val1 = abs(tempx - ((struct driver *)ptr)->x);
    int val2 = abs(tempy - ((struct driver *)ptr)->y);
    
    ((struct driver *)ptr)->x = tempx;
    ((struct driver *)ptr)->y = tempy;
    
    useconds_t time = (val1 + val2) * (250 * 1000);

    printf("\t  * %s: starting order %d, heading to restaurant %d (travel time = %d ms)\n", ((struct driver *)ptr)->name, myorder, ORDERS[myorder].x,(time / (1000)));
    
    strcpy(((struct driver *)ptr)->status, "busy");

    usleep(time);

    // Travel to customer
    int tempx1 = PEOPLE_COORDS[ORDERS[myorder].y].x;
    int tempy1 = PEOPLE_COORDS[ORDERS[myorder].y].y;
    
    int val3 = abs(tempx1 - ((struct driver *)ptr)->x);
    int val4 = abs(tempy1 - ((struct driver *)ptr)->y);
    
    ((struct driver *)ptr)->x = tempx1;
    ((struct driver *)ptr)->y = tempy1;
    
    time = (val3 + val4) * (250 * 1000);

    usleep(time);
    
    printf("%s: finished order %d.\n",((struct driver *)ptr)->name, myorder);
    
    ((struct driver *)ptr)->count = ((struct driver *)ptr)->count + 1;
    strcpy(((struct driver *)ptr)->status, "idle");
  }

  pthread_exit(NULL);
  
  return NULL;
}

// Restaurant function to create the orders for the delivery thread
// takes in a pointer of type coordinate
void *cook(void *ptr){
  char buf[80];
  
  while(1){
    pthread_mutex_lock(&lock);
    if(fgets(buf, 80, INFILE) == NULL)
      break;
    
    int n = sscanf(buf, "%d %d", &ORDERS[ORDER_NUM].x, &ORDERS[ORDER_NUM].y);

    if(n != 2){
      fprintf(stdout, "Read error in customer coords.\n");
      exit(6);
    }

    if(ORDERS[ORDER_NUM].x >= 0 && ORDERS[ORDER_NUM].y >= 0){
      sem_post(&NUM_ORDERS);
      ORDER_NUM++;
    }
    else{
      // Sleep for 250 milliseconds
      usleep(250 * 1000);
    }
    
    pthread_mutex_unlock(&lock);
  }

  pthread_exit(NULL);
 
  return NULL;
}

// Main function to handle initialize and data parsing and thread creation.
int main(int argc, char *argv[]){

  // Error checking
  if(argc != 3){
    fprintf(stdout, "Invalid number of arguments, must be 2.\n");
    exit(1);
  }

  INFILE = fopen(argv[1], "r");

  if(INFILE == NULL){
    fprintf(stdout, "Invalid file.\n");
    exit(2);
  }

  // Data initialization
  int counter = 0;
  char buf[80];
  int num_drivers = 0;

  pthread_t restaurant;

  pthread_mutex_init(&lock, NULL);
  sem_init(&NUM_ORDERS, 0, 0);
  
  struct driver drivers[num_drivers + 1];

  ORDER_NUM = 0;
  MAX_TIME = atoi(argv[2]);

  // Loop to parse through data file.
  while(1){
    if(fgets(buf, 80, INFILE) == NULL)
      break;
    
    if(counter == 0){
      num_drivers = atoi(buf);

      if(num_drivers < 1){
	fprintf(stdout, "Invalid number of drivers.\n");
	exit(3);
      }
    }
    
    if(counter > 0 && counter < 11){
      int n = sscanf(buf, "%d %d", &REST_COORDS[counter - 1].x, &REST_COORDS[counter - 1].y);

      if(n != 2){
	fprintf(stdout, "Read error in lines 2-11.\n");
	exit(4);
      }

      fprintf(stdout, "Restaurant %d is at %d, %d\n", counter-1, REST_COORDS[counter - 1].x, REST_COORDS[counter - 1].y);
    }
    
    if(counter > 10 && counter < 21){
      int n = sscanf(buf, "%d %d", &PEOPLE_COORDS[counter - 11].x, &PEOPLE_COORDS[counter - 11].y);

      if(n != 2){
	fprintf(stdout, "Read error in lines 12-21.\n");
	exit(4);
      }

      fprintf(stdout, "Customer %d is at %d, %d\n", counter-11, PEOPLE_COORDS[counter - 11].x, PEOPLE_COORDS[counter - 11].y);
    }

    if(counter > 20){
      break;
    }
    
    counter++;
  }

  // Now create threads
  pthread_t thread[num_drivers];
  
  int i;
  char a[100];
  for(i = 0; i < num_drivers; i++){
    
    // Add number to name
    sprintf(a, "%s %d", "Driver", i+1);
    
    strcpy(drivers[i+1].name, a);
   
    drivers[i+1].count = 0;
    drivers[i+1].x = 0;
    drivers[i+1].y = 0;

    strcpy(drivers[i+1].status, "idle");
    
    int ret = pthread_create(&thread[i], NULL, deliver, (void *)&drivers[i+1]);
    
    if(ret != 0){
      fprintf(stdout, "pthread_creation failed with error code %d.\n", ret);
      exit(5);
    }
  }

  int ret1 = pthread_create(&restaurant, NULL, cook, (void *)NULL);
  
  if(ret1 != 0){
    fprintf(stdout, "pthread_creation failed with error code %d.\n", ret1);
    exit(5);
  }
    
  int total;
  int timeout = MAX_TIME;
  
  int j;
  int k;
  for(i = 0; i <= timeout; i++){
    printf("After %d seconds:\n", i);

    total = 0;
    for(j = 0; j < num_drivers; j++){
      printf("\t%s: %s, completed %d orders\n", drivers[j+1].name, drivers[j+1].status, drivers[j+1].count);
      total += drivers[j+1].count;
    }

    printf("\tTotal completed deliveries: %d\n", total);
    
    usleep(1000000);
    MAX_TIME--;
    
    if(i == timeout){
      for(k = 0; k < num_drivers; k++){
	pthread_kill(thread[k], 0);
      }
      pthread_kill(restaurant, 0);
      break;
    }    
  }
  
  sem_destroy(&NUM_ORDERS);
  pthread_mutex_destroy(&lock);
    
  return 0;
}
