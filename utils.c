#include "utils.h"

#include <stdlib.h>

int coords_to_var(int width, int row, int col) {
  return (row*width) + col +1;//Lógica para converter um array 2D em 1D, 
}//+1 para evitar a variável 0 ,já que o DIMACS não usa variável 0.
int var_to_coords(int var, int* row, int* col) {
    // Silencia os avisos do compilador fingindo que usamos as variáveis, já que não as usei.
    (void)var;
    (void)row;
    (void)col;
    return -1; 
}

char* copy_cadical_output(FILE* cadical_file)
{
  size_t cap = 4096;
  size_t len = 0;
  char* output = malloc(cap);
  int character;

  while ((character = fgetc(cadical_file)) != EOF) {
    if (len + 1 >= cap) {
      cap *= 2;
      output = realloc(output, cap);
    }
    output[len++] = (char)character;
  }

  output[len] = '\0';

  return output;
}
