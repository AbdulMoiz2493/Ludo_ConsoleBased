#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>

#define BOARD_SIZE 15
#define NUM_PLAYERS 4
#define MAX_TOKENS 4

// Global variables for shared resources
char board[BOARD_SIZE][BOARD_SIZE];
pthread_mutex_t board_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dice_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t dice_semaphore;
sem_t board_semaphore;

// Player structure
typedef struct {
    int id;
    char symbol;
    char color[10];
    int tokens[MAX_TOKENS][2];  // Position of each token [row][col]
    pthread_t thread_id;
} Player;

Player players[NUM_PLAYERS];
int num_tokens_per_player;

void initialize_board() {
    pthread_mutex_lock(&board_mutex);
    
    // Clear the board first
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = ' ';
        }
    }
    
    // Initialize home areas
    for(int i = 0; i < 6; i++) {
        for(int j = 0; j < 6; j++) {
            board[i][j] = 'R';  // Red home
        }
    }
    
    for(int i = 0; i < 6; i++) {
        for(int j = 9; j < 15; j++) {
            board[i][j] = 'Y';  // Yellow home
        }
    }
    
    for(int i = 9; i < 15; i++) {
        for(int j = 9; j < 15; j++) {
            board[i][j] = 'G';  // Green home
        }
    }
    
    for(int i = 9; i < 15; i++) {
        for(int j = 0; j < 6; j++) {
            board[i][j] = 'B';  // Blue home
        }
    }
    
    // Initialize safe squares
    board[6][1] = 'S';
    board[1][8] = 'S';
    board[8][13] = 'S';
    board[13][6] = 'S';
    
    // Initialize paths with dots
     // Yellow safe path
    int yellow_safe[][2] = {{1,7}, {2,7}, {3,7}, {4,7}, {5,7}, {6,7}, {6,8}, {1,8}, {2,6}};
    for(int i = 0; i < sizeof(yellow_safe)/sizeof(yellow_safe[0]); i++) {
        board[yellow_safe[i][0]][yellow_safe[i][1]] = 'y';  // lowercase for dots
    }
    
    // Blue safe path
    int blue_safe[][2] = {{7,7}, {8,7}, {9,7}, {10,7}, {11,7}, {12,7}, {8,6}, {13,6}, {12,8}, {13,7}};
    for(int i = 0; i < sizeof(blue_safe)/sizeof(blue_safe[0]); i++) {
        board[blue_safe[i][0]][blue_safe[i][1]] = 'b';
    }
    
    // Red safe path
    int red_safe[][2] = {{7,1}, {7,2}, {7,3}, {7,4}, {7,5}, {7,6}, {6,6}, {6,1}, {8,2}};
    for(int i = 0; i < sizeof(red_safe)/sizeof(red_safe[0]); i++) {
        board[red_safe[i][0]][red_safe[i][1]] = 'r';
    }
    
    // Green safe path
    int green_safe[][2] = {{7,8}, {7,9}, {7,10}, {7,11}, {7,12}, {7,13}, {8,13}, {8,8}, {6,12}};
    for(int i = 0; i < sizeof(green_safe)/sizeof(green_safe[0]); i++) {
        board[green_safe[i][0]][green_safe[i][1]] = 'g';
    }
    
    pthread_mutex_unlock(&board_mutex);
}

void initialize_players() {
    printf("Enter number of tokens per player (1-4): ");
    scanf("%d", &num_tokens_per_player);
    
    if(num_tokens_per_player < 1 || num_tokens_per_player > 4) {
        printf("Invalid number of tokens. Setting to default (4)\n");
        num_tokens_per_player = 4;
    }
    
    char colors[4][10] = {"Red", "Yellow", "Green", "Blue"};
    char symbols[4] = {'R', 'Y', 'G', 'B'};
    
    for(int i = 0; i < NUM_PLAYERS; i++) {
        players[i].id = i;
        players[i].symbol = symbols[i];
        strcpy(players[i].color, colors[i]);
        
        // Initialize token positions in home yards
        for(int j = 0; j < num_tokens_per_player; j++) {
            switch(i) {
                case 0: // Red
                    players[i].tokens[j][0] = 2 + (j/2);
                    players[i].tokens[j][1] = 2 + (j%2);
                    break;
                case 1: // Yellow
                    players[i].tokens[j][0] = 2 + (j/2);
                    players[i].tokens[j][1] = 11 + (j%2);
                    break;
                case 2: // Green
                    players[i].tokens[j][0] = 11 + (j/2);
                    players[i].tokens[j][1] = 11 + (j%2);
                    break;
                case 3: // Blue
                    players[i].tokens[j][0] = 11 + (j/2);
                    players[i].tokens[j][1] = 2 + (j%2);
                    break;
            }
        }
    }
}

void display_board() {
    pthread_mutex_lock(&board_mutex);
    
    printf("\n  ");
    for(int j = 0; j < BOARD_SIZE; j++) {
        //printf(" %2d", j);
    }
    printf("\n");
    
    for(int i = 0; i < BOARD_SIZE; i++) {
        //printf("%2d ", i);
        for(int j = 0; j < BOARD_SIZE; j++) {
            int token_present = 0;
            for(int p = 0; p < NUM_PLAYERS; p++) {
                
                for(int t = 0; t < num_tokens_per_player; t++) {
                    if(players[p].tokens[t][0] == i && players[p].tokens[t][1] == j) {
                        printf("\033[1;%dm%c \033[0m", 
                               (p == 0) ? 31 :  // Red
                               (p == 1) ? 33 :  // Yellow
                               (p == 2) ? 32 :  // Green
                               34,              // Blue
                               players[p].symbol);
                        token_present = 1;
                        break;
                    }
                }
                if(token_present) break;
            }
            
            if(!token_present) {
                switch(board[i][j]) {
                    case 'R': printf("\033[1;31m█ \033[0m"); break;
                    case 'Y': printf("\033[1;33m█ \033[0m"); break;
                    case 'G': printf("\033[1;32m█ \033[0m"); break;
                    case 'B': printf("\033[1;34m█ \033[0m"); break;
                    case 'r': printf("\033[1;31m• \033[0m"); break;
                    case 'y': printf("\033[1;33m• \033[0m"); break;
                    case 'g': printf("\033[1;32m• \033[0m"); break;
                    case 'b': printf("\033[1;34m• \033[0m"); break;
                    case 'S': printf("\033[1;37m* \033[0m"); break;
                    default: printf("□ ");
                }
            }
        }
        printf("\n");
    }
    
    pthread_mutex_unlock(&board_mutex);
}

int main() {
    // Initialize random seed
    srand(time(NULL));
    
    // Initialize semaphores
    sem_init(&dice_semaphore, 0, 1);
    sem_init(&board_semaphore, 0, 1);
    
    // Initialize the game board and players
    initialize_board();
    initialize_players();
    
    // Display initial board state
    printf("\nInitial Ludo Board State:\n");
    display_board();
    
    // Display board legend
    printf("\nBoard Legend:\n");
    printf("\033[1;31m█ \033[0m- Red Home\n");
    printf("\033[1;33m█ \033[0m- Yellow Home\n");
    printf("\033[1;32m█ \033[0m- Green Home\n");
    printf("\033[1;34m█ \033[0m- Blue Home\n");
    printf("\033[1;37m* \033[0m- Safe Square\n");
    printf("□ - Path\n");
    
    // Clean up resources
    sem_destroy(&dice_semaphore);
    sem_destroy(&board_semaphore);
    pthread_mutex_destroy(&board_mutex);
    pthread_mutex_destroy(&dice_mutex);
    
    return 0;
}