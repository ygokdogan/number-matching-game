#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define USERNAME_LEN 50
#define MOVE_STACK_CAPACITY 500

typedef struct {
  int **grid;
  int size;
} Board;

typedef enum { UP, DOWN, RIGHT, LEFT, INVALID } MoveDirection;

typedef struct {
  int fromRow, fromCol;
  int toRow, toCol;
  int distance;
  MoveDirection direction;
  int targetValue;
} Move;

typedef struct {
  Move *moves;
  int top;
  int capacity;
} MoveStack;

typedef enum { AUTO, MANUAL } MatchingMode;
typedef enum { RANDOM, READ } BoardSource;

typedef struct {
  char username[USERNAME_LEN];
  double score;
  int undoCount;
  int *matchesDone;

  MatchingMode mode;
  BoardSource boardSource;

  Board board;
  MoveStack moveHistory;

  time_t startTime;
  time_t endTime;
} GameSession;

typedef struct {
  char username[USERNAME_LEN];
  double bestScore;
  int totalGames;
  GameSession *gameHistory;
} Player;

typedef struct {
  Player *players;
  int count;
  Player bestPlayer;
} PlayerList;

typedef struct {
  int startRow, startCol;
  int endRow, endCol;
  int isFound;
} Endpoint;

typedef struct {
  // Succesful Messages
  const char *boardFilled;
  const char *undoSuccessful;
  const char *saveSuccessful;
  const char *loadSuccessful;

  // Warning Messages
  const char *diagonalMovement;
  const char *outOfBoundMove;
  const char *pathNotClear;
  const char *emptySourceCell;
  const char *moveHistoryEmpty;

  // Error Messages
  const char *allocationFailed;
  const char *fileNotFound;
  const char *playerNotFound;
  const char *fileCantCreated;
  const char *loadFailed;
} Messages;

void printMainMenu();
void mainMenu(PlayerList *playerList);
int gameMenu(GameSession *game, Player *player, PlayerList *playerList);
void modeMenu(Player *player, PlayerList *playerList, BoardSource source);
char *printEnterUserName();
Player *switchPlayer(PlayerList *playerList);

void savePlayers(PlayerList list, const char *filename);
void loadPlayers(PlayerList *list, const char *filename);
void saveGameSession(GameSession *game, const char *filename);
int loadGameSession(GameSession *game, const char *filename);

void initBoard(GameSession *game, int n, int initNumber);
void generateWinnableBoard(GameSession *game);
int readBoardFromFile(GameSession *game, const char *filename);
void printBoard(GameSession *game);

void initMoveStack(GameSession *game);
Move pushToHistory(GameSession *game, Move move);
Move popFromHistory(GameSession *game);
Move peekToHistory(GameSession *game);
int isMoveHistoryEmpty(GameSession *game);

int checkWin(GameSession *game);
int solvePathRecursive(GameSession *game, Endpoint *endpoints, int currentNum, int currRow, int currCol, int maxNum);
int autoSolve(GameSession *game);

void makeMove(GameSession *game, Move move, int match);
void undoMove(GameSession *game);
int isValidMove(Board board, Move *move);
int checkPathRecursive(Board board, int currRow, int currCol, int toRow,
                       int toCol, MoveDirection dir, int sourceNumber);
int calculateDistance(Move move);
MoveDirection calculateDirection(Move move);

Player *addPlayer(PlayerList *playerList, const char *username);
Player *findPlayer(PlayerList playerList, const char *username);
Player *addFinishedGameToPlayer(Player *player, GameSession game);
void displayPlayerInfo(PlayerList playerList, const char *username);
void listPlayersBestScores(PlayerList playerList);
int displayGameHistory(Player player);
double calculateScore(Player player, GameSession *game);

Messages getMessages();
void freeResources(GameSession *game, Player player);
void freeGameBoard(GameSession *game);

int main() {
  int i;

  PlayerList playerList;
  playerList.count = 0;
  playerList.players = NULL;

  srand((unsigned int)time(NULL));

  loadPlayers(&playerList, "players.bin");

  printf("=== Welcome to Number Matching Game ===\n");
  {
    mainMenu(&playerList);
  }

  savePlayers(playerList, "players.bin");

  for (i = 0; i < playerList.count; i++) {
    free(playerList.players[i].gameHistory);
  }
  free(playerList.players);

  return 0;
}

//Prints the main menu
void printMainMenu() {
  printf("\n ========= MAIN MENU ===========\n");
  printf(" 1. Create Random Matrix\n");
  printf(" 2. Create Matrix From File\n");
  printf(" 3. Display All Player Scores\n");
  printf(" 4. Display Current Player Details\n");
  printf(" 5. Load Saved Game\n");
  printf(" 0. Exit\n\n");
}

//Operates the main menu, playerList is for switching players or listing best players.
void mainMenu(PlayerList *playerList) {
  int choice = -1;
  int running = 1;
  Player *currPlayer = NULL;

  while (running) {
    printMainMenu();
    printf(" Choice: ");
    scanf("%d", &choice);

    switch (choice) {
    case 1:
      currPlayer = switchPlayer(playerList);
      modeMenu(currPlayer, playerList, RANDOM);
      break;
    case 2:
      currPlayer = switchPlayer(playerList);
      modeMenu(currPlayer, playerList, READ);
      break;
    case 3:
      listPlayersBestScores(*playerList);
      break;
    case 4:
      if (currPlayer == NULL) {
        currPlayer = switchPlayer(playerList);
      }
      displayPlayerInfo(*playerList, currPlayer->username);
      break;
    case 5:
      GameSession savedGame;
      char savefile[USERNAME_LEN + 20];
      if (currPlayer == NULL) {
        currPlayer = switchPlayer(playerList);
      }
      sprintf(savefile, "%s_save.bin", currPlayer->username);

      if (loadGameSession(&savedGame, savefile)) {
        gameMenu(&savedGame, currPlayer, playerList);
        freeGameBoard(&savedGame);
      }
      break;
    case 0:
      running = 0;
      break;
    default:
      printf(" Invalid choice. Please try again.\n");
      break;
    }
  }
}

//Operates the game mode menu by getting player, playerList and source inputs to initalize board with player and source's info
void modeMenu(Player *player, PlayerList *playerList, BoardSource source) {
  int choice = -1;
  int running = 1;

  while (running) {
    printf("\n ========= GAME MODE ===========\n");
    printf(" 1. Play in Manual Mode\n");
    printf(" 2. Play in Automatic Mode\n");
    printf(" 0. Return to Main Menu\n");
    printf(" Choice: ");
    scanf("%d", &choice);

    if (choice == 1 || choice == 2) {
      int valid = 1;
      int n;
      char filename[100];
      GameSession game;

      game.undoCount = 0;
      game.score = 0.0;
      game.boardSource = source;
      game.mode = (choice == 1) ? MANUAL : AUTO;
      strncpy(game.username, player->username, USERNAME_LEN - 1);
      game.username[USERNAME_LEN - 1] = '\0';
      initMoveStack(&game);
      time(&game.startTime);

      if (source == RANDOM) {
        printf(" Enter board size N (min 3): ");
        scanf("%d", &n);
        if (n < 3) {
          n = 3;
        }
        initBoard(&game, n, 0);
        generateWinnableBoard(&game);
      } else {
        printf(" Enter filename: ");
        scanf("%99s", filename);
        valid = readBoardFromFile(&game, filename);
        if (!valid) {
          free(game.moveHistory.moves);
        }
      }

      if (valid) {
        game.undoCount = 0;
        gameMenu(&game, player, playerList);
        freeGameBoard(&game);
        running = 0;
      }
    } else if (choice == 0) {
      running = 0;
    } else {
      printf(" Invalid choice.\n");
    }
  }
}

//Operates the game menu of given gameSession, player and playerList inputs
int gameMenu(GameSession *game, Player *player, PlayerList *playerList) {
  int choice = -1;
  int running = 1;
  int won = 0;
  int srcnumber;
  Move currentMove;
  char savefile[USERNAME_LEN + 20];

  sprintf(savefile, "%s_save.bin", player->username);

  while (running) {
    printBoard(game);

    printf("========== GAME MENU ==========\n");
    if (game->mode == MANUAL) {
      printf("[1] Make a Move\n");
      printf("[2] Undo Last Move\n");
      printf("[3] Save Game\n");
    } else {
      printf("[1] Solve Board\n");
      printf("[2] Save Game\n");
    }
    printf("[0] Save and Return to Main Menu\n");
    printf("Choice: ");
    scanf("%d", &choice);

    if (game->mode == MANUAL) {
      switch (choice) {
      case 1:
        printf("--> Enter source cell (Row Col): ");
        scanf("%d %d", &currentMove.fromRow, &currentMove.fromCol);
        printf("--> Enter target cell (Row Col): ");
        scanf("%d %d", &currentMove.toRow, &currentMove.toCol);

        currentMove.fromRow--;
        currentMove.fromCol--;
        currentMove.toRow--;
        currentMove.toCol--;

        makeMove(game, currentMove, 1);

        if (checkWin(game) == 1) {
          printBoard(game);
          printf("\n Congratulations %s! You won!\n", player->username);
          time(&game->endTime);
          game->score = calculateScore(*player, game);
          printf(" Your score: %.2f\n", game->score);
          addFinishedGameToPlayer(player, *game);
          savePlayers(*playerList, "players.bin");
          won = 1;
          running = 0;
        }
        break;
      case 2:
        undoMove(game);
        break;
      case 3:
        saveGameSession(game, savefile);
        break;
      case 0:
        saveGameSession(game, savefile);
        running = 0;
        break;
      default:
        printf(" Invalid choice.\n");
        break;
      }
    } else {
      switch (choice) {
      case 1:
        if (autoSolve(game)) {
          printBoard(game);
          printf("\n Congratulations %s! You won!\n", player->username);
          time(&game->endTime);
          game->score = calculateScore(*player, game);
          printf(" Your score: %.2f\n", game->score);
          addFinishedGameToPlayer(player, *game);
          savePlayers(*playerList, "players.bin");
          won = 1;
          running = 0;
        } else {
          printf("\n Board couldn't be solved.\n");
        }
        break;
      case 2:
        saveGameSession(game, savefile);
        break;
      case 0:
        saveGameSession(game, savefile);
        running = 0;
        break;
      }
    }
  }
  return won;
}

//Prints the enter userName menu and returns the gotten username
char *printEnterUserName() {
  char *name = (char *)malloc(USERNAME_LEN * sizeof(char));
  printf("\n========= ENTER USERNAME ===========\n");
  printf(" Enter username (min 3 chars): ");
  scanf("%49s", name);
  return name;
}

//Switches the player by getting a username from the user. Then adds it to playerList
Player *switchPlayer(PlayerList *playerList) {
  char *username;
  int valid = 0;
  Player *newPlayer;

  while (!valid) {
    username = printEnterUserName();
    if ((int)strlen(username) >= 3) {
      valid = 1;
    } else {
      printf(" Username must be at least 3 characters.\n");
      free(username);
    }
  }
  newPlayer = addPlayer(playerList, username);
  free(username);

  return newPlayer;
}

//Adds a new player with the given username to the playerlist. If a player with that username exists it just returns that player
Player *addPlayer(PlayerList *playerList, const char *username) {
  Player *player = findPlayer(*playerList, username);

  if (player == NULL) {
    Player *tempPlayers = (Player *)realloc(
        playerList->players, (playerList->count + 1) * sizeof(Player));
    if (tempPlayers == NULL) {
      printf("Error in addPlayer(): %s", getMessages().allocationFailed);
      return NULL;
    }
    playerList->players = tempPlayers;
    player = &playerList->players[playerList->count];

    player->totalGames = 0;
    player->bestScore = 0;

    player->gameHistory = NULL;

    strncpy(player->username, username, USERNAME_LEN - 1);
    player->username[USERNAME_LEN - 1] = '\0';

    playerList->count++;
  }
  return player;
}

//Adds the given gameSession to given players game history
Player *addFinishedGameToPlayer(Player *player, GameSession game) {
  player->gameHistory = (GameSession *)realloc(
      player->gameHistory, (player->totalGames + 1) * sizeof(GameSession));

  if (player->gameHistory == NULL) {
    printf("Error in addPlayer(): %s", getMessages().allocationFailed);
    return NULL;
  }
  if (game.score > player->bestScore) {
    player->bestScore = game.score;
  }
  player->gameHistory[player->totalGames] = game;
  player->totalGames++;
  return player;
}

Player *findPlayer(PlayerList playerList, const char *username) {
  int i;
  for (i = 0; i < playerList.count; i++) {
    if (strcmp(playerList.players[i].username, username) == 0) {
      return &playerList.players[i];
    }
  }

  return NULL;
}

//Displays player info by given username in the playerList
void displayPlayerInfo(PlayerList playerList, const char *username) {
  Player *playerPtr = findPlayer(playerList, username);

  if (playerPtr == NULL) {
    printf("%s", getMessages().playerNotFound);
    return;
  }

  Player player = *playerPtr;

  printf("\n========== PLAYER INFO ==========\n");
  printf(" Username: %s\n", player.username);
  printf(" Best Score: %.2f\n", player.bestScore);
  printf(" Total Game Count: %d", player.totalGames);
  displayGameHistory(player);
}

//Lists all players and their best scores in playerList
void listPlayersBestScores(PlayerList playerList) {
  int i;
  printf("\n========== PLAYERS AND THEIR BEST SCORES ==========\n");
  for (i = 0; i < playerList.count; i++) {
    Player currPlayer = playerList.players[i];
    printf(" %d. %s: %lf\n", i+1, currPlayer.username, currPlayer.bestScore);
  }
}

//Displays given player's game history with detail
int displayGameHistory(Player player) {
  int i;
  if (player.totalGames <= 0) {
    return 0;
  }
  for (i = 0; i < player.totalGames; i++) {
    GameSession currentSession = player.gameHistory[i];
    double duration =
        difftime(currentSession.endTime, currentSession.startTime);

    char startTimeStr[64];
    char endTimeStr[64];
    char *modeText = "";

    struct tm *startInfo = localtime(&currentSession.startTime);
    strftime(startTimeStr, sizeof(startTimeStr), "%d.%m.%Y %H:%M:%S",
             startInfo);

    struct tm *endInfo = localtime(&currentSession.endTime);
    strftime(endTimeStr, sizeof(endTimeStr), "%d.%m.%Y %H:%M:%S", endInfo);

    if (currentSession.mode == MANUAL)
      modeText = "MANUAL";
    else
      modeText = "AUTO";

    printf("\n--- Game #%d ---\n", i + 1);
    printf(" Score        : %.2f\n", currentSession.score);
    printf(" Undo Count   : %d\n", currentSession.undoCount);
    printf(" Mode         : %s\n", modeText);
    printf(" Start        : %s\n", startTimeStr);
    printf(" Finish       : %s\n", endTimeStr);
    printf(" Game Duration: %.0f seconds\n", duration);
    printf("---------------------------------------------\n\n");
  }

  return 1;
}

// Checks if the game is finished by looking the matchesDone array and looking
// if there is an empty cell in matrix
int checkWin(GameSession *game) {
  int i, j;
  for (i = 0; i < game->board.size; i++) {
    if (game->matchesDone[i] != 1) {
      return 0;
    }

    for (j = 0; j < game->board.size; j++) {
      if (game->board.grid[i][j] <= 0) {
        return 0;
      }
    }
  }

  return 1;
}

//Calculates the score of the game for the player 
double calculateScore(Player player, GameSession *game) {
  double duration = difftime(game->endTime, game->startTime);
  if (duration < 1.0) {
    duration = 1.0;
  }

  double baseScore = game->board.size * game->board.size * 500.0;
  double modeMultiplier = (game->mode == MANUAL) ? 1.0 : 0.05;
  double sourceMultiplier = (game->boardSource == RANDOM) ? 1.0 : 0.8;
  double totalPenalty = duration + (game->undoCount * 5.0);
  double experienceMultiplier = 1.0 + (player.totalGames * 0.02);
  double finalScore = (baseScore / totalPenalty) * modeMultiplier *
                      sourceMultiplier * experienceMultiplier;
  if (finalScore < 0.0) {
    finalScore = 0.0;
  }

  return finalScore;
}

// Solves the paths recursively by checking all directions and backtracking, a
// need for generating a winnable board
int solvePathRecursive(GameSession *game, Endpoint *endpoints, int currentNum,
                       int currRow, int currCol, int maxNum) {
  int i;
  int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

  if (currentNum > maxNum) {
    return checkWin(game);
  }

  if (endpoints[currentNum].isFound == 0) {
    if (currentNum + 1 > maxNum)
      return checkWin(game);
    return solvePathRecursive(game, endpoints, currentNum + 1,
                              endpoints[currentNum + 1].startRow,
                              endpoints[currentNum + 1].startCol, maxNum);
  }

  if (currRow == endpoints[currentNum].endRow &&
      currCol == endpoints[currentNum].endCol) {
    game->matchesDone[currentNum - 1] = 1;

    int nextNum = currentNum + 1;
    if (nextNum <= maxNum) {
      if (solvePathRecursive(game, endpoints, nextNum,
                             endpoints[nextNum].startRow,
                             endpoints[nextNum].startCol, maxNum)) {
        return 1;
      }
    } else {
      if (checkWin(game))
        return 1;
    }

    game->matchesDone[currentNum - 1] = 0;
    return 0;
  }

  for (i = 0; i < 4; i++) {
    int nextRow = currRow + dirs[i][0];
    int nextCol = currCol + dirs[i][1];

    if (nextRow >= 0 && nextRow < game->board.size && nextCol >= 0 &&
        nextCol < game->board.size) {

      if (nextRow == endpoints[currentNum].endRow &&
          nextCol == endpoints[currentNum].endCol) {
        if (solvePathRecursive(game, endpoints, currentNum, nextRow, nextCol,
                               maxNum)) {
          return 1;
        }
      } else if (game->board.grid[nextRow][nextCol] == 0) {
        Move nextMove;
        nextMove.fromCol = currCol;
        nextMove.fromRow = currRow;
        nextMove.toCol = nextCol;
        nextMove.toRow = nextRow;

        printf("  [%d] (%d,%d) -> (%d,%d)\n", currentNum, currRow + 1,
               currCol + 1, nextRow + 1, nextCol + 1);
        makeMove(game, nextMove, 0);

        if (solvePathRecursive(game, endpoints, currentNum, nextRow, nextCol,
                               maxNum)) {
          return 1;
        }

        printf("  [%d] UNDO (%d,%d)\n", currentNum, nextRow + 1, nextCol + 1);
        undoMove(game);
      }
    }
  }

  return 0;
}

// Scans the board, finds the endpoints and solves all of them recursively
int autoSolve(GameSession *game) {
  int i, j;
  int maxNum = 0;
  Endpoint *endpoints =
      (Endpoint *)calloc(game->board.size + 1, sizeof(Endpoint));

  for (i = 0; i < game->board.size; i++) {
    for (j = 0; j < game->board.size; j++) {
      int cellValue = game->board.grid[i][j];

      if (cellValue > 0) {
        if (cellValue > maxNum)
          maxNum = cellValue;

        if (endpoints[cellValue].isFound == 0) {
          endpoints[cellValue].startRow = i;
          endpoints[cellValue].startCol = j;
          endpoints[cellValue].isFound = 1;
        } else {
          endpoints[cellValue].endRow = i;
          endpoints[cellValue].endCol = j;
        }
      }
    }
  }

  if (maxNum == 0)
    return 0;

  int result = solvePathRecursive(game, endpoints, 1, endpoints[1].startRow,
                                  endpoints[1].startCol, maxNum);
  free(endpoints);
  return result;
}

// Checks if the move is valid and if so fills in between target and source
// points with the sourceNumber.
void makeMove(GameSession *game, Move move, int match) {
  if (isValidMove(game->board, &move) != 1)
    return;

  int currCol = move.fromCol;
  int currRow = move.fromRow;
  int sourceNumber = game->board.grid[currRow][currCol];

  move.targetValue = game->board.grid[move.toRow][move.toCol];

  while (currCol != move.toCol || currRow != move.toRow) {
    switch (move.direction) {
    case UP:
      currRow--;
      break;
    case DOWN:
      currRow++;
      break;
    case RIGHT:
      currCol++;
      break;
    case LEFT:
      currCol--;
      break;
    default:
      return;
    }

    if (game->board.grid[currRow][currCol] == 0) {
      game->board.grid[currRow][currCol] = sourceNumber;
    }
  }

  if (move.targetValue == sourceNumber && match) {
    game->matchesDone[move.targetValue - 1] = 1;
  }

  pushToHistory(game, move);
}

//Undo the last move made in the given game session
void undoMove(GameSession *game) {
  if (isMoveHistoryEmpty(game) == 1) {
    printf("Warning: %s", getMessages().moveHistoryEmpty);
    return;
  }

  Move last = popFromHistory(game);

  int currCol = last.fromCol;
  int currRow = last.fromRow;

  int sourceNumber = game->board.grid[last.fromRow][last.fromCol];

  if (last.targetValue == sourceNumber) {
    game->matchesDone[last.targetValue - 1] = 0;
  }

  while (currCol != last.toCol || currRow != last.toRow) {
    switch (last.direction) {
    case UP:
      currRow--;
      break;
    case DOWN:
      currRow++;
      break;
    case RIGHT:
      currCol++;
      break;
    case LEFT:
      currCol--;
      break;
    default:
      return;
    }

    if (currRow == last.toRow && currCol == last.toCol) {
      game->board.grid[currRow][currCol] = last.targetValue;
    } else {
      game->board.grid[currRow][currCol] = 0;
    }
  }

  game->undoCount++;
  printf("System: %s", getMessages().undoSuccessful);
}

// Checks if the move is valid
int isValidMove(Board board, Move *move) {
  if (move->fromRow < 0 || move->fromRow >= board.size || move->fromCol < 0 ||
      move->fromCol >= board.size || move->toRow < 0 ||
      move->toRow >= board.size || move->toCol < 0 ||
      move->toCol >= board.size) {
    printf("Error: %s", getMessages().outOfBoundMove);
    return 0;
  }

  if (board.grid[move->fromRow][move->fromCol] <= 0) {
    printf("Error: %s", getMessages().emptySourceCell);
    return 0;
  }

  move->direction = calculateDirection(*move);
  if (move->direction == INVALID) {
    printf("Error: %s", getMessages().diagonalMovement);
    return 0;
  }

  int startRow = move->fromRow;
  int startCol = move->fromCol;
  switch (move->direction) {
  case UP:
    startRow--;
    break;
  case DOWN:
    startRow++;
    break;
  case LEFT:
    startCol--;
    break;
  case RIGHT:
    startCol++;
    break;
  }

  int sourceNumber = board.grid[move->fromRow][move->fromCol];
  int isPathClear =
      checkPathRecursive(board, startRow, startCol, move->toRow, move->toCol,
                         move->direction, sourceNumber);
  if (isPathClear == 0) {
    printf("Error: %s", getMessages().pathNotClear);
    return 0;
  }

  move->distance = calculateDistance(*move);

  return 1;
}

// Check the path in between source and target to be empty using recursion
int checkPathRecursive(Board board, int currRow, int currCol, int toRow,
                       int toCol, MoveDirection dir, int sourceNumber) {
  if (currRow == toRow && currCol == toCol &&
      (board.grid[currRow][currCol] == 0 ||
       board.grid[currRow][currCol] == sourceNumber)) {
    return 1;
  }

  if (board.grid[currRow][currCol] != 0) {
    return 0;
  }

  int nextRow = currRow;
  int nextCol = currCol;

  switch (dir) {
  case UP:
    nextRow--;
    break;
  case DOWN:
    nextRow++;
    break;
  case LEFT:
    nextCol--;
    break;
  case RIGHT:
    nextCol++;
    break;
  default:
    return 0;
  }

  return checkPathRecursive(board, nextRow, nextCol, toRow, toCol, dir,
                            sourceNumber);
}

// Calculates the direction of the move made
MoveDirection calculateDirection(Move move) {
  int fromR = move.fromRow, fromC = move.fromCol;
  int toR = move.toRow, toC = move.toCol;

  if (fromR == toR && toC > fromC)
    return RIGHT;
  if (fromR == toR && toC < fromC)
    return LEFT;
  if (fromC == toC && toR > fromR)
    return DOWN;
  if (fromC == toC && toR < fromR)
    return UP;
  return INVALID;
}

// Calculates the distance of the move made
int calculateDistance(Move move) {
  int fromR = move.fromRow, fromC = move.fromCol;
  int toR = move.toRow, toC = move.toCol;

  if (move.direction == RIGHT || move.direction == LEFT)
    return abs(toC - fromC);
  if (move.direction == DOWN || move.direction == UP)
    return abs(toR - fromR);
  return 0;
}

// Initializes the board with size of (n) and given initialization number.
void initBoard(GameSession *game, int n, int initNumber) {
  int i, j;
  game->board.size = n;
  game->board.grid = (int **)malloc(n * sizeof(int *));
  game->matchesDone = (int *)calloc(n, sizeof(int));

  if (game->board.grid == NULL) {
    fprintf(stderr, "Error in initBoard(): %s.",
            getMessages().allocationFailed);
    exit(1);
  }

  for (i = 0; i < game->board.size; i++) {
    game->board.grid[i] = (int *)malloc(n * sizeof(int));

    if (game->board.grid[i] == NULL) {
      fprintf(stderr, "Error in initBoard(): %s",
              getMessages().allocationFailed);
      exit(1);
    }

    for (j = 0; j < game->board.size; j++) {
      game->board.grid[i][j] = initNumber;
    }
  }
}

// Generates a winnable baord for the given session by checking if it is
// solvable after creating a board
void generateWinnableBoard(GameSession *game) {
  int i, j;
  int n = game->board.size;
  int solvable = 0;
  int attempts = 0;

  int **backupGrid = (int **)malloc(n * sizeof(int *));
  for (i = 0; i < n; i++) {
    backupGrid[i] = (int *)malloc(n * sizeof(int));
  }

  while (solvable != 1 && attempts < 10000) {
    attempts++;
    for (i = 0; i < n; i++) {
      game->matchesDone[i] = 0;
      for (j = 0; j < n; j++) {
        game->board.grid[i][j] = 0;
      }
    }

    int currentNumber = 1;
    int count = 0;
    while (currentNumber <= n) {
      int row = rand() % n;
      int col = rand() % n;

      if (game->board.grid[row][col] == 0) {
        game->board.grid[row][col] = currentNumber;
        count++;
        if (count >= 2) {
          currentNumber++;
          count = 0;
        }
      }
    }

    for (i = 0; i < n; i++) {
      for (j = 0; j < n; j++) {
        backupGrid[i][j] = game->board.grid[i][j];
      }
    }

    if (autoSolve(game) == 1) {
      solvable = 1;
    }
  }

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      game->board.grid[i][j] = backupGrid[i][j];
    }
    game->matchesDone[i] = 0;
  }

  for (i = 0; i < n; i++) {
    free(backupGrid[i]);
  }
  free(backupGrid);

  printf("%s", getMessages().boardFilled);
}

//Reads board for given gameSession from a file with the filename given
int readBoardFromFile(GameSession *game, const char *filename) {
  FILE *file = fopen(filename, "r");
  int i, j, n;
  int** matrix;
  int a, b, num;

  if (!file) {
    printf("Error: %s", getMessages().fileNotFound);
    return 0;
  }
  
  matrix = (int**) malloc(100 * sizeof(int*));
  for(i = 0; i < 100; i++) {
    matrix[i] = (int*) malloc(100 * sizeof(int));
  }

  n=0;
  int maxDim = 0;
  while (fscanf(file, "%d %d %d", &a, &b, &num) == 3) {
    matrix[a][b] = num;
    if (a > maxDim) maxDim = a;
    if (b > maxDim) maxDim = b;
  }

  n = maxDim + 1;
  initBoard(game, n, 0);

  for(i = 0; i < n; i++) {
    for(j = 0; j < n; j++) {
      game->board.grid[i][j] = matrix[i][j];
    }
  }

  for(i = 0; i < 100; i++) {
    free(matrix[i]);
  }
  free(matrix);
  fclose(file);
  printf(" Board loaded from '%s'.\n", filename);
  return 1;
}

// Prints the given session's board
void printBoard(GameSession *game) {
  int i, j;
  int n = game->board.size;

  printf("\n\t   === CURRENT BOARD ===\n\n");

  printf("        ");
  for (j = 0; j < n; j++) {
    printf("%2d  ", j + 1);
  }
  printf("\n");

  printf("       +");
  for (j = 0; j < n; j++) {
    printf("---+");
  }
  printf("\n");

  for (i = 0; i < n; i++) {
    printf("  %2d   |", i + 1);

    for (j = 0; j < n; j++) {
      int cellValue = game->board.grid[i][j];

      if (cellValue == 0) {
        printf(" . |");
      } else {
        printf("%2d |", cellValue);
      }
    }
    printf("\n");

    printf("       +");
    for (j = 0; j < n; j++) {
      printf("---+");
    }
    printf("\n");
  }
  printf("\n");
  printf("Matched numbers: ");
  for (i = 0; i < game->board.size; i++) {
    if (game->matchesDone[i] != 0) {
      printf("%d ", i + 1);
    }
  }
  printf("\n");
}

// Initializes move stack and allocates memory for it
void initMoveStack(GameSession *game) {
  game->moveHistory.capacity = MOVE_STACK_CAPACITY;
  game->moveHistory.top = -1;
  game->moveHistory.moves = malloc(game->moveHistory.capacity * sizeof(Move));

  if (game->moveHistory.moves == NULL) {
    fprintf(stderr,
            "Error in initMoveStack() %s: ", getMessages().allocationFailed);
    exit(1);
  }
}

// Checks if move history is empty
int isMoveHistoryEmpty(GameSession *game) {
  return game->moveHistory.top < 0 ? 1 : 0;
}

// Pushes a move to given session's move history
Move pushToHistory(GameSession *game, Move move) {
  if (game->moveHistory.top + 1 >= game->moveHistory.capacity) {
    int newCapacity = game->moveHistory.capacity * 2;
    Move *temp =
        (Move *)realloc(game->moveHistory.moves, newCapacity * sizeof(Move));

    if (temp == NULL) {
      printf("Error in pushToHistory(): %s", getMessages().allocationFailed);
      exit(1);
    }
    game->moveHistory.moves = temp;
    game->moveHistory.capacity = newCapacity;
  }

  game->moveHistory.moves[++game->moveHistory.top] = move;
  return move;
}

// Pops a move from given session's move history
Move popFromHistory(GameSession *game) {
  if (game->moveHistory.top < 0) {
    Move invalidMove = {-1, -1, -1, -1, 0, INVALID, 0};
    return invalidMove;
  }
  return game->moveHistory.moves[game->moveHistory.top--];
}

// Returns to last move made in given session's move history
Move peekToHistory(GameSession *game) {
  if (game->moveHistory.top < 0) {
    Move invalidMove = {-1, -1, -1, -1, 0, INVALID, 0};
    return invalidMove;
  }
  return game->moveHistory.moves[game->moveHistory.top];
}

// Saves the current game session to a binary file
void saveGameSession(GameSession *game, const char *filename) {
  FILE *file = fopen(filename, "wb");
  int i, j;
  if (file == NULL) {
    printf("Error: %s", getMessages().fileCantCreated);
    return;
  }

  fwrite(game->username, sizeof(char), USERNAME_LEN, file);
  fwrite(&game->undoCount, sizeof(int), 1, file);
  fwrite(&game->mode, sizeof(MatchingMode), 1, file);
  fwrite(&game->boardSource, sizeof(BoardSource), 1, file);
  fwrite(&game->startTime, sizeof(time_t), 1, file);

  fwrite(&game->board.size, sizeof(int), 1, file);
  for (i = 0; i < game->board.size; i++) {
    for (j = 0; j < game->board.size; j++) {
      fwrite(&game->board.grid[i][j], sizeof(int), 1, file);
    }
  }

  for (i = 0; i < game->board.size; i++) {
    fwrite(&game->matchesDone[i], sizeof(int), 1, file);
  }

  fwrite(&game->moveHistory.top, sizeof(int), 1, file);
  fwrite(&game->moveHistory.capacity, sizeof(int), 1, file);
  for (i = 0; i <= game->moveHistory.top; i++) {
    fwrite(&game->moveHistory.moves[i], sizeof(Move), 1, file);
  }

  fclose(file);
  printf("System: %s", getMessages().saveSuccessful);
}

// Loads the game session from given binary file
int loadGameSession(GameSession *game, const char *filename) {
  FILE *file = fopen(filename, "rb");
  int i, j;
  if (file == NULL) {
    printf("Error: %s", getMessages().loadFailed);
    return 0;
  }

  fread(game->username, sizeof(char), USERNAME_LEN, file);
  fread(&game->undoCount, sizeof(int), 1, file);
  fread(&game->mode, sizeof(MatchingMode), 1, file);
  fread(&game->boardSource, sizeof(BoardSource), 1, file);
  fread(&game->startTime, sizeof(time_t), 1, file);

  int size;
  fread(&size, sizeof(int), 1, file);

  initBoard(game, size, 0);
  for (i = 0; i < game->board.size; i++) {
    for (j = 0; j < game->board.size; j++) {
      fread(&game->board.grid[i][j], sizeof(int), 1, file);
    }
  }

  for (i = 0; i < game->board.size; i++) {
    fread(&game->matchesDone[i], sizeof(int), 1, file);
  }

  int top, capacity;
  fread(&top, sizeof(int), 1, file);
  fread(&capacity, sizeof(int), 1, file);

  game->moveHistory.top = top;
  game->moveHistory.capacity = capacity;
  game->moveHistory.moves = (Move *)malloc(capacity * sizeof(Move));

  for (i = 0; i <= game->moveHistory.top; i++) {
    fread(&game->moveHistory.moves[i], sizeof(Move), 1, file);
  }

  fclose(file);
  printf("System: %s", getMessages().loadSuccessful);
  return 1;
}

// Saves all players to a binary file
void savePlayers(PlayerList list, const char *filename) {
  FILE *file = fopen(filename, "wb");
  int i, j;
  if (file == NULL) {
    printf("Error: %s\n", getMessages().fileCantCreated);
    return;
  }

  fwrite(&list.count, sizeof(int), 1, file);

  for (i = 0; i < list.count; i++) {
    Player p = list.players[i];

    fwrite(p.username, sizeof(char), USERNAME_LEN, file);
    fwrite(&p.bestScore, sizeof(double), 1, file);
    fwrite(&p.totalGames, sizeof(int), 1, file);

    for (j = 0; j < p.totalGames; j++) {
      fwrite(&p.gameHistory[j].score, sizeof(double), 1, file);
      fwrite(&p.gameHistory[j].undoCount, sizeof(int), 1, file);
      fwrite(&p.gameHistory[j].mode, sizeof(MatchingMode), 1, file);
      fwrite(&p.gameHistory[j].startTime, sizeof(time_t), 1, file);
      fwrite(&p.gameHistory[j].endTime, sizeof(time_t), 1, file);
    }
  }

  fclose(file);
}

// Loads all players from a binary file
void loadPlayers(PlayerList *list, const char *filename) {
  int i, j;
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    list->count = 0;
    list->players = NULL;
    return;
  }

  fread(&list->count, sizeof(int), 1, file);
  if (list->count > 0) {
    list->players = (Player *)malloc(list->count * sizeof(Player));
    for (i = 0; i < list->count; i++) {
      Player *p = &list->players[i];
      fread(&p->username, sizeof(char), USERNAME_LEN, file);
      fread(&p->bestScore, sizeof(double), 1, file);
      fread(&p->totalGames, sizeof(int), 1, file);

      if (p->totalGames > 0) {
        p->gameHistory =
            (GameSession *)malloc(p->totalGames * sizeof(GameSession));
        for (j = 0; j < p->totalGames; j++) {
          fread(&p->gameHistory[j].score, sizeof(double), 1, file);
          fread(&p->gameHistory[j].undoCount, sizeof(int), 1, file);
          fread(&p->gameHistory[j].mode, sizeof(MatchingMode), 1, file);
          fread(&p->gameHistory[j].startTime, sizeof(time_t), 1, file);
          fread(&p->gameHistory[j].endTime, sizeof(time_t), 1, file);

          p->gameHistory[j].board.grid = NULL;
          p->gameHistory[j].moveHistory.moves = NULL;
        }
      } else {
        p->gameHistory = NULL;
      }
    }
  }

  fclose(file);
}

// Gets pre defined messages to print to user interface
Messages getMessages() {
  Messages msg = {
      // Successful
      "Board Filled Successfully (Winnable).\n",
      "Move successfully undone.\n",
      "Save successful.\n",
      "Load successful.\n",

      // Warning
      "You can only move vertical and horizontally.\n",
      "The coordinates you entered are out of bound, please try again.\n",
      "The path is not clear, please try again.\n",
      "Source cell is empty, please choose non-empty cell to make a move.\n",
      "There is no move to undo.\n",

      // Error
      "Memory allocation failed.\n",
      "File not found.\n",
      "Player not found.\n",
      "File cannot be created.\n",
      "Load failed.\n",
  };
  return msg;
}

// Frees the game board
void freeGameBoard(GameSession *game) {
  int i, j;
  free(game->moveHistory.moves);
  free(game->matchesDone);
  for (i = 0; i < game->board.size; i++) {
    free(game->board.grid[i]);
  }
  free(game->board.grid);
}

// Frees all the resources used in the process
void freeResources(GameSession *game, Player player) {
  freeGameBoard(game);
  free(player.gameHistory);
}