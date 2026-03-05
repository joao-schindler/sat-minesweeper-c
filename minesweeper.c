#include "minesweeper.h"

#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

minesweeper_t* ms_create(char* initial_grid_path, char* ground_truth_path)
{
  int rows, cols;
  int** ground_truth_grid = parse_ground_truth(ground_truth_path, &rows, &cols);
  int** initial_grid = parse_initial_grid(initial_grid_path, rows, cols);

  minesweeper_t* ms = malloc(sizeof(minesweeper_t));

  int** revealed_grid = malloc((rows) * sizeof(int*));
  for (int i = 0; i < rows; ++i) revealed_grid[i] = malloc((cols) * sizeof(int));

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      if (initial_grid[i][j] != -1)
        revealed_grid[i][j] = 1;
      else
        revealed_grid[i][j] = 0;
    }
  }

  ms->rows = rows;
  ms->cols = cols;
  ms->player_grid = initial_grid;
  ms->ground_truth_grid = ground_truth_grid;
  ms->revealed_grid = revealed_grid;

  return ms;
}

int ms_in_bounds(int i, int j, int rows, int cols)
{
  return i >= 0 && i < rows && j >= 0 && j < cols;
}

void ms_reveal_recursion(minesweeper_t* ms, int r, int c)
{
  if (!ms_in_bounds(r, c, ms->rows, ms->cols) || ms->revealed_grid[r][c] == 1) return;
  if (ms->ground_truth_grid[r][c] > 0) {
    ms->revealed_grid[r][c] = 1;
    ms->player_grid[r][c] = ms->ground_truth_grid[r][c];
  } else if (ms->ground_truth_grid[r][c] == 0) {
    ms->revealed_grid[r][c] = 1;
    ms->player_grid[r][c] = ms->ground_truth_grid[r][c];
    ms_reveal_recursion(ms, r - 1, c);
    ms_reveal_recursion(ms, r + 1, c);
    ms_reveal_recursion(ms, r, c - 1);
    ms_reveal_recursion(ms, r, c + 1);
  }
}

int ms_reveal(minesweeper_t* ms, int r, int c)
{
  if (!ms_in_bounds(r, c, ms->rows, ms->cols)) {
    fprintf(stderr, "Célula [%d][%d] fora dos limites do tabuleiro!\n", r, c);
    exit(1);
  }

  if (ms->ground_truth_grid[r][c] == -1) return 0;

  ms_reveal_recursion(ms, r, c);

  return 1;
}

void ms_oracle_cell_decision(minesweeper_t* ms, int* r, int* c)
{
  for (int i = 0; i < ms->rows; ++i) {
    if (*r != -1 && *c != -1) break;
    for (int j = 0; j < ms->cols; ++j) {
      if (ms->revealed_grid[i][j] == 0 && ms->ground_truth_grid[i][j] != -1) {
        *r = i;
        *c = j;
        break;
      }
    }
  }
}

int ms_check_win(minesweeper_t* ms)
{
  for (int i = 0; i < ms->rows; ++i)
    for (int j = 0; j < ms->cols; ++j)
      if (ms->ground_truth_grid[i][j] != -1 && ms->revealed_grid[i][j] == 0) return 0;
  return 1;
}

void ms_destroy(minesweeper_t* ms)
{
  for (int i = 0; i < ms->rows; ++i) {
    free(ms->player_grid[i]);
    free(ms->ground_truth_grid[i]);
    free(ms->revealed_grid[i]);
  }
  free(ms->player_grid);
  free(ms->ground_truth_grid);
  free(ms->revealed_grid);
  free(ms);
}

void ms_print(minesweeper_t* ms)
{
  for (int i = 0; i < ms->rows; ++i) {
    for (int j = 0; j < ms->cols; ++j) {
      if (ms->revealed_grid[i][j] == 1) {
        if (ms->ground_truth_grid[i][j] == 0)
          printf("· ");
        else
          printf("%d ", ms->player_grid[i][j]);
      } else {
        printf("□ ");
      }
    }
    printf("\n");
  }
}
