#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define Print(Str) fputs(Str, stdout)
#define WIDTH 10
#define HEIGHT 20
#define MAX_COLOR 14
#define SELECTED_COLOR 15
/*

L ###
  #

J ###
    #

O ##
  ##

I ####


S  ##
  ##

Z ##
   ##

T ###
   #

N = nothing

*/
enum Piece { PC_L, PC_J, PC_O, PC_I, PC_S, PC_Z, PC_T, PC_N };
typedef enum Piece Piece;
int Offsets[PC_N][6] = {0, -1, 0, 1,  1, -1, 0, -1, 0, 1,  1, 1, 0, 1,
                        1, 0,  1, 1,  0, -1, 0, 1,  0, 2,  0, 1, 1, 0,
                        1, -1, 0, -1, 1, 0,  1, 1,  0, -1, 0, 1, 1, 0};
static Piece Active = PC_N;
static int Rows[4];
static int Columns[4];
static int NextColor = 1;
static size_t Score;
static size_t Level = 1;
static size_t Lines;
int Game[HEIGHT][WIDTH];
void AddScore(size_t Amt) {
  Score = Score > SIZE_MAX - Amt ? SIZE_MAX : Score + Amt;
  Level = Score >= 123000 ? 42 : Score / 3000 + 1;
}
void Hide(void) {
  Print("\e[?1049h\e[?25l");
  if (isatty(STDIN_FILENO)) {
    system("stty -echo -icanon time 0 min 0");
  }
}
void Clean(void) {
  Print("\e[?1049l\e[?25h");
  if (isatty(STDIN_FILENO)) {
    system("stty sane");
  }
}
volatile sig_atomic_t ShouldQuit;
void QuitSignal(int Signal) { ShouldQuit = 1; }
void StaticText(void) {
  printf("\e[1;%iHr = restart\e[2;%iH^C = quit\e[H", WIDTH * 2 + 4,
         WIDTH * 2 + 4);
  for (int Row = 0; Row < HEIGHT; ++Row) {
    printf("|\e[%iC|\e[1E", WIDTH * 2);
  }
  putchar('+');
  for (int Column = 0; Column < WIDTH; ++Column) {
    Print("--");
  }
  Print("+\e[1EScore:\e[1ELevel:\e[1ELines:");
}
void Draw(void) {
  for (int Row = 0; Row < HEIGHT; ++Row) {
    printf("\e[%zu;2H", Row + 1);
    int *Displaying = Game[Row];
    for (int Column = 0; Column < WIDTH; ++Column) {
      int Color = Displaying[Column];
      printf("\e[48;5;%um %c", Color, Color ? ' ' : '.');
    }
  }
  printf("\e[48;5;0m\e[%i;8H%zu\e[%i;8H%zu\e[%i;8H%zu", HEIGHT + 2, Score,
         HEIGHT + 3, Level, HEIGHT + 4, Lines);
}
bool CreatePiece(Piece Creating) {
  int Row1 = 0;
  int Column1 = 4;
  int Row2 = Row1 + *Offsets[Creating];
  int Column2 = Column1 + Offsets[Creating][1];
  int Row3 = Row1 + Offsets[Creating][2];
  int Column3 = Column1 + Offsets[Creating][3];
  int Row4 = Row1 + Offsets[Creating][4];
  int Column4 = Column1 + Offsets[Creating][5];
  if (Game[Row1][Column1] || Game[Row2][Column2] || Game[Row3][Column3] ||
      Game[Row4][Column4]) {
    return 1;
  }
  *Rows = Row1;
  Rows[1] = Row2;
  Rows[2] = Row3;
  Rows[3] = Row4;
  *Columns = Column1;
  Columns[1] = Column2;
  Columns[2] = Column3;
  Columns[3] = Column4;
  Game[Row1][Column1] = SELECTED_COLOR;
  Game[Row2][Column2] = SELECTED_COLOR;
  Game[Row3][Column3] = SELECTED_COLOR;
  Game[Row4][Column4] = SELECTED_COLOR;
  Active = Creating;
  return 0;
}
bool IsFilled(int Color) { return Color && Color <= MAX_COLOR; }
bool IsGrounded(int Row, int Column) {
  return Row == HEIGHT - 1 || IsFilled(Game[Row + 1][Column]);
}
void SetColor(int Color) {
  Game[*Rows][*Columns] = Color;
  Game[Rows[1]][Columns[1]] = Color;
  Game[Rows[2]][Columns[2]] = Color;
  Game[Rows[3]][Columns[3]] = Color;
}
void MovePiece(void) {
  if (IsGrounded(*Rows, *Columns) || IsGrounded(Rows[1], Columns[1]) ||
      IsGrounded(Rows[2], Columns[2]) || IsGrounded(Rows[3], Columns[3])) {
    SetColor(NextColor);
    NextColor = NextColor % MAX_COLOR + 1;
    Active = PC_N;
    int Lowest;
    int Highest;
    if (*Rows < Rows[1]) {
      Lowest = *Rows;
      Highest = Rows[1];
    } else {
      Lowest = Rows[1];
      Highest = *Rows;
    }
    Lowest = Rows[2] < Lowest ? Rows[2] : Lowest;
    Lowest = Rows[3] < Lowest ? Rows[3] : Lowest;
    Highest = Rows[2] > Highest ? Rows[2] : Highest;
    Highest = Rows[3] > Highest ? Rows[3] : Highest;
    size_t OldLines = Lines;
    for (int Row = Lowest; Row <= Highest; ++Row) {
      int *Examining = Game[Row];
      for (int Column = 0; Column < WIDTH; ++Column) {
        if (!Examining[Column]) {
          goto NotFull;
        }
      }
      memmove(Game + 1, Game, sizeof(int) * WIDTH * Row);
      memset(Game, 0, sizeof(int) * WIDTH);
      if (Lines != SIZE_MAX) {
        ++Lines;
      }
    NotFull:;
    }
    AddScore((size_t[]){0, 40, 100, 300, 1200}[Lines - OldLines] * Level);
    return;
  }
  SetColor(0);
  *Rows += 1;
  Rows[1] += 1;
  Rows[2] += 1;
  Rows[3] += 1;
  SetColor(SELECTED_COLOR);
}
bool CantMoveLeft(int Row, int Column) {
  return !Column || IsFilled(Game[Row][Column - 1]);
}
void MoveLeft(void) {
  if (CantMoveLeft(*Rows, *Columns) || CantMoveLeft(Rows[1], Columns[1]) ||
      CantMoveLeft(Rows[2], Columns[2]) || CantMoveLeft(Rows[3], Columns[3])) {
    return;
  }
  SetColor(0);
  *Columns -= 1;
  Columns[1] -= 1;
  Columns[2] -= 1;
  Columns[3] -= 1;
  SetColor(SELECTED_COLOR);
}
bool CantMoveRight(int Row, int Column) {
  return Column == WIDTH - 1 || IsFilled(Game[Row][Column + 1]);
}
void MoveRight(void) {
  if (CantMoveRight(*Rows, *Columns) || CantMoveRight(Rows[1], Columns[1]) ||
      CantMoveRight(Rows[2], Columns[2]) ||
      CantMoveRight(Rows[3], Columns[3])) {
    return;
  }
  SetColor(0);
  *Columns += 1;
  Columns[1] += 1;
  Columns[2] += 1;
  Columns[3] += 1;
  SetColor(SELECTED_COLOR);
}
bool CantMoveTo(int Row, int Column) {
  return Row < 0 || Column < 0 || Row >= HEIGHT || Column >= WIDTH ||
         IsFilled(Game[Row][Column]);
}
void Rotate(void) {
  int Rows1 = *Rows + Columns[1] - *Columns;
  int Columns1 = *Columns + *Rows - Rows[1];
  int Rows2 = *Rows + Columns[2] - *Columns;
  int Columns2 = *Columns + *Rows - Rows[2];
  int Rows3 = *Rows + Columns[3] - *Columns;
  int Columns3 = *Columns + *Rows - Rows[3];
  if (CantMoveTo(Rows1, Columns1) || CantMoveTo(Rows2, Columns2) ||
      CantMoveTo(Rows3, Columns3)) {
    return;
  }
  SetColor(0);
  Rows[1] = Rows1;
  Rows[2] = Rows2;
  Rows[3] = Rows3;
  Columns[1] = Columns1;
  Columns[2] = Columns2;
  Columns[3] = Columns3;
  SetColor(SELECTED_COLOR);
}
long GetInterval(void) {
  return 900000000 - 50000000L * (Level < 18 ? Level - 1 : 17);
}
void Play(void) {
  struct timespec Before;
  clock_gettime(CLOCK_MONOTONIC, &Before);
  for (;;) {
    if (nanosleep(&(struct timespec){0, 10000000}, 0) && errno != EINTR) {
      exit(EXIT_FAILURE);
    }
    if (ShouldQuit) {
      exit(EXIT_SUCCESS);
    }
    struct timespec After;
    clock_gettime(CLOCK_MONOTONIC, &After);
    for (int Input;
         (Input = getchar()) != EOF || !feof(stdin) && !ferror(stdin);) {
      switch (Input) {
      case 'r': {
        return;
      }
      case 'a': {
        if (Active != PC_N) {
          MoveLeft();
        }
        break;
      }
      case 'd': {
        if (Active != PC_N) {
          MoveRight();
        }
        break;
      }
      case 'w': {
        if (Active != PC_N) {
          Rotate();
        }
        break;
      }
      case 's': {
        if (Active == PC_N) {
          break;
        }
        MovePiece();
        AddScore(1);
        long Interval = GetInterval();
        if (Before.tv_nsec < 1000000000 - Interval) {
          Before.tv_nsec = After.tv_sec == Before.tv_sec &&
                                   After.tv_nsec <= Before.tv_nsec + Interval
                               ? After.tv_nsec
                               : Before.tv_nsec + Interval;
        } else if (Before.tv_sec == After.tv_sec) {
          Before.tv_nsec = After.tv_nsec;
        } else if (++Before.tv_sec == After.tv_sec &
                   (Before.tv_nsec -= 1000000000 - Interval) > After.tv_nsec) {
          Before.tv_nsec = After.tv_nsec;
        }
        break;
      }
      }
    }
    for (;;) {
      long Interval = GetInterval();
      if (Before.tv_nsec < 1000000000 - Interval) {
        if (Before.tv_sec == After.tv_sec &&
            After.tv_nsec < Before.tv_nsec + Interval) {
          break;
        }
        Before.tv_nsec += Interval;
      } else {
        if (Before.tv_sec == After.tv_sec ||
            Before.tv_sec + 1 == After.tv_sec &&
                After.tv_nsec < Before.tv_nsec - (1000000000 - Interval)) {
          break;
        }
        ++Before.tv_sec;
        Before.tv_nsec -= 1000000000 - Interval;
      }
      if (Active == PC_N) {
        if (CreatePiece(random() % PC_N)) {
          return;
        }
      } else {
        MovePiece();
      }
    }
    Draw();
    fflush(stdout);
  }
}
int main(void) {
  struct timespec Start;
  if (clock_gettime(CLOCK_REALTIME, &Start)) {
    return EXIT_FAILURE;
  }
  srandom((unsigned)Start.tv_sec * 1000 + (unsigned)(Start.tv_nsec / 1000000));
  if (signal(SIGINT, QuitSignal) == SIG_ERR ||
      signal(SIGTERM, QuitSignal) == SIG_ERR ||
      signal(SIGQUIT, QuitSignal) == SIG_ERR || signal(SIGTSTP, QuitSignal)) {
    return EXIT_FAILURE;
  }
  atexit(Clean);
  Hide();
  for (;;) {
    StaticText();
    Draw();
    fflush(stdout);
    if (nanosleep(&(struct timespec){3}, 0) && errno != EINTR) {
      return EXIT_FAILURE;
    }
    if (ShouldQuit) {
      return EXIT_SUCCESS;
    }
    for (int Input;
         (Input = getchar()) != EOF || !feof(stdin) && !ferror(stdin);)
      ;
    Play();
    Print("\e[2J");
    NextColor = 1;
    Level = 1;
    Score = Lines = 0;
    Active = PC_N;
    memset(Game, 0, sizeof(Game));
  }
  return EXIT_SUCCESS;
}
