/* Benjamin Decre
 * bed1@umbc.edu
 *
 * Test program for driver 
 * 
 * Notes: this must be run as sudo
 */

#include <stdio.h>

#include "cs421net.h"

int main(void) {
  char *buf = NULL;
  int bufSize = 33;
  int check = 0;
  cs421net_init();
  
  /* Test 1: Sending too much data */
  cs421net_send(buf, bufSize);

  /* Test 2: checking if a regular user can write to master */
  check += system("echo -n bad_user > /dev/pwkeeper_master");

  /* Test 3: check if sudoer can write to master */
  check += system("sudo echo -n good_user > /dev/pwkeeper_master");

  /* Test 4: checking is regular user can write to account */
  check += system("echo -n reg_user1 > /dev/pwkeeper_account");

  /* Test 5: checking if a second account can register as well */
  check += system("echo -n reg_user2 > /dev/pwkeeper_account");
   
  /* Test 6: testing if a second account can write to master */
  check += system("sudo echo -n good_user_boogaloo > /dev/pwkeeper_master");

  /* Test 7: attempting to change password as master */
  check += system("sudo echo -n good_user > /dev/pwkeeper_master");

  /* Test 8: attempting to change password as account */
  check += system("echo -n reg_user1 > /dev/pwkeeper_account");

  /* Test 9: attempting to write someone elses password as user */
  check += system("echo -n reg_user1 >/dev/pwkeeper_master");

  /* Test 10: attempting to write someone elses password as master */
  check += system("echo -n good_user > /dev/pwkeeper_account");

  if(check > 0)
    printf("one or more tests failed.");
  return 0;
}
