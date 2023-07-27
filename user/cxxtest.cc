/*!
 * \file
 * \brief test allocation alignments
 */

#include "user/user.h"


void main() {
  printf("writing to 0x00 ... ");
  *((char *)0x00) = 'a';
  printf("success\n");
  return;
}
