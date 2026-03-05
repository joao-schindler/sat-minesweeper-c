#include "parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int** parse_ground_truth(char* ground_truth_path, int* width, int* height)
{
  FILE* ground_truth_file = fopen(ground_truth_path, "r");
  if (!ground_truth_file) {
    fprintf(stderr, "Falha ao ler o arquivo com a posição das bombas.\n");
    exit(1);
  }

  fscanf(ground_truth_file, "%d", width);
  *height = *width;

  int** grid = malloc((*height) * sizeof(int*));
  for (int i = 0; i < *height; ++i) grid[i] = malloc((*width) * sizeof(int));

  for (int i = 0; i < *height; ++i)
    for (int j = 0; j < *width; ++j) fscanf(ground_truth_file, "%d", &grid[i][j]);

  fclose(ground_truth_file);

  return grid;
}

int** parse_initial_grid(char* initial_grid_path, int width, int height)
{
  FILE* initial_grid_file = fopen(initial_grid_path, "r");
  if (!initial_grid_file) {
    fprintf(stderr, "Falha ao ler o arquivo com o estado inicial do jogador.\n");
    exit(1);
  }

  int** grid = malloc((height) * sizeof(int*));
  for (int i = 0; i < height; ++i) grid[i] = malloc((width) * sizeof(int));

  for (int i = 0; i < height; ++i)
    for (int j = 0; j < width; ++j) fscanf(initial_grid_file, "%d", &grid[i][j]);

  fclose(initial_grid_file);

  return grid;
}

int* parse_cadical_output(char* output, int* model_size)
{
  int* model = NULL;
  size_t mcap = 128;
  size_t mlen = 0;
  model = malloc(mcap * sizeof(int));
  if (!model) {
    fprintf(stderr, "\nFalha ao alocar memória para o modelo...\n");
    free(output);
    exit(1);
  }

  char* p = output;
  while (*p) {
    if (*p == 'v' && (p[1] == ' ' || p[1] == '\t')) {
      char* q = p + 1;
      while (*q == ' ' || *q == '\t') ++q;

      while (*q) {
        errno = 0;
        char* end;
        long lit = strtol(q, &end, 10);
        if (q == end) break;
        if (errno == ERANGE) {
          fprintf(stderr, "\nNúmero fora do range no modelo...\n");
          free(model);
          free(output);
          exit(1);
        }
        if (lit == 0) {
          q = end;
          break;
        }

        if (mlen >= mcap) {
          mcap *= 2;
          int* tmp = realloc(model, mcap * sizeof(int));
          if (!tmp) {
            fprintf(stderr, "\nFalha ao realocar memória para o modelo...\n");
            free(model);
            free(output);
            exit(1);
          }
          model = tmp;
        }
        model[mlen++] = (int)lit;

        q = end;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') ++q;
      }
      p = q;
    } else {
      ++p;
    }
  }

  *model_size = (int)mlen;

  if (mlen == 0) {
    free(model);
    return NULL;
  } else {
    int* shrink = realloc(model, mlen * sizeof(int));
    if (shrink) model = shrink;
    return model;
  }
}
