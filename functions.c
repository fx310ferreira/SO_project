#include <string.h>

int str_validator(char* str, int under_score){
  int i;
  for (i=0; i < (int)strlen(str); i++) {
    if((str[i] < 'a' || str[i] > 'z') && (str[i] < 'A' || str[i] > 'Z') && (str[i] < '0' || str[i] > '9') && (!under_score || (under_score && (str[i] != '_')))){
      return 0;
    }
  }
  return 1;
}
