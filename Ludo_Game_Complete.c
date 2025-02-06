#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>

#define BOARD_SIZE 15
#define NUM_PLAYERS 4
#define MAX_TOKENS 4
#define PATH_LENGTH 52

pthread_mutex_t board_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dice_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t dice_semaphore;
sem_t board_semaphore;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;
int previous_turn = -1;

typedef struct {
    int id;
    char symbol;
    char color[10];
    int tokens[MAX_TOKENS][2];
    int token_positions[MAX_TOKENS];
    int hit_record;
    int home_tokens;
    pthread_t thread_id;
    bool is_active;
    int num_tokens;
    int rank;
} Player;

typedef struct {
    int player1_id;
    int player2_id;
    bool has_team;
} Team;

typedef struct {
    int team_number;
    char player1_color[10];
    char player2_color[10];
    int total_hits;
    int rank;
} TeamResult;

Team teams[2];  // For 2 teams of 2 players each
bool team_mode = false;


// Global variables
char board[BOARD_SIZE][BOARD_SIZE];
Player players[NUM_PLAYERS];
int num_tokens_per_player;
int active_players = NUM_PLAYERS;
int current_rank = 1;

// Function declarations
void initialize_board(void);
void initialize_players(void);
bool are_teammates(int player1_id, int player2_id);
bool teammate_finished(Player* player);
void display_board(void);
int roll_dice(void);
bool is_safe_square(int row, int col);
bool has_killed_token(Player* player);
bool can_enter_home(Player* player);
bool is_token_home(Player* player, int token_idx);
bool can_move_token(Player* player, int token_idx, int steps);
void check_hits(Player* player, int new_row, int new_col);
void move_token(Player* player, int token_idx, int steps);
bool check_win(Player* player);
void* player_turn(void* arg);


// Add this function to initialize teams
void initialize_teams() {
    char choice;
    printf("Do you want to play in team mode? (y/n): ");
    scanf(" %c", &choice);
    
    if(choice == 'y' || choice == 'Y') {
        team_mode = true;
        printf("\nForming teams...\n");
        teams[0].player1_id = 0;  // Red
        teams[0].player2_id = 1;  // Yellow
        teams[0].has_team = true;
        
        teams[1].player1_id = 2;  // Green
        teams[1].player2_id = 3;  // Blue
        teams[1].has_team = true;
        
        printf("Team 1: %s and %s\n", players[0].color, players[1].color);
        printf("Team 2: %s and %s\n", players[2].color, players[3].color);
    }
}

// Add function to check if two players are teammates
bool are_teammates(int player1_id, int player2_id) {
    if(!team_mode) return false;
    
    for(int i = 0; i < 2; i++) {
        if((teams[i].player1_id == player1_id && teams[i].player2_id == player2_id) ||
           (teams[i].player1_id == player2_id && teams[i].player2_id == player1_id)) {
            return true;
        }
    }
    return false;
}

// Add function to check if a player's teammate has finished
bool teammate_finished(Player* player) {
    if(!team_mode) return false;
    
    for(int i = 0; i < 2; i++) {
        if(teams[i].player1_id == player->id) {
            return players[teams[i].player2_id].home_tokens == players[teams[i].player2_id].num_tokens;
        }
        if(teams[i].player2_id == player->id) {
            return players[teams[i].player1_id].home_tokens == players[teams[i].player1_id].num_tokens;
        }
    }
    return false;
}


const int path_coords[4][PATH_LENGTH][2] = {
    // Red player path (starts at 6,1)
    {{6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {5,6}, {4,6}, {3,6}, {2,6}, {1,6}, {0,6},
     {0,7}, {0,8}, {1,8}, {2,8}, {3,8}, {4,8}, {5,8}, {6,9}, {6,10}, {6,11}, {6,12}, {6,13}, {6,14},
     {7,14}, {8,14}, {8,13}, {8,12}, {8,11}, {8,10}, {8,9}, {9,8}, {10,8}, {11,8}, {12,8}, {13,8}, {14,8},
     {14,7}, {14,6}, {13,6}, {12,6}, {11,6}, {10,6}, {9,6}, {8,5}, {8,4}, {8,3}, {8,2}, {8,1}, {8,0}, {7,0}, {6,0}},
    
    // Yellow player path (starts at 1,8)
    {{1,8}, {2,8}, {3,8}, {4,8}, {5,8}, {6,9}, {6,10}, {6,11}, {6,12}, {6,13}, {6,14},
     {7,14}, {8,14}, {8,13}, {8,12}, {8,11}, {8,10}, {8,9}, {9,8}, {10,8}, {11,8}, {12,8}, {13,8}, {14,8},
     {14,7}, {14,6}, {13,6}, {12,6}, {11,6}, {10,6}, {9,6}, {8,5}, {8,4}, {8,3}, {8,2}, {8,1}, {8,0}, {7,0}, {6,0},
     {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {5,6}, {4,6}, {3,6}, {2,6}, {1,6}, {0,6}, {0,7}, {0,8}},
    
    // Green player path (starts at 8,13)
{{8,13}, {8,12}, {8,11}, {8,10}, {8,9}, {9,8}, {10,8}, {11,8}, {12,8}, {13,8}, {14,8},
{14,7}, {14,6}, {13,6}, {12,6}, {11,6}, {10,6}, {9,6}, {8,5}, {8,4}, {8,3}, {8,2}, {8,1}, {8,0},
{7,0}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {5,6}, {4,6}, {3,6}, {2,6}, {1,6}, {0,6},
{0,7}, {0,8}, {1,8}, {2,8}, {3,8}, {4,8}, {5,8}, {6,9}, {6,10}, {6,11}, {6,12}, {6,13}, {6,14}, {7,14}, {8,14}},

// Blue player path (starts at 13,6)
{{13,6}, {12,6}, {11,6}, {10,6}, {9,6}, {8,5}, {8,4}, {8,3}, {8,2}, {8,1}, {8,0},
{7,0}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {5,6}, {4,6}, {3,6}, {2,6}, {1,6}, {0,6},
{0,7}, {0,8}, {1,8}, {2,8}, {3,8}, {4,8}, {5,8}, {6,9}, {6,10}, {6,11}, {6,12}, {6,13}, {6,14},
{7,14}, {8,14}, {8,13}, {8,12}, {8,11}, {8,10}, {8,9}, {9,8}, {10,8}, {11,8}, {12,8}, {13,8}, {14,8}, {14,7}, {14,6}}
};

// Add new function to check if a player has killed at least one token
bool has_killed_token(Player* player) {
    return player->hit_record > 0;
}

bool team_has_killed(Player* player) {
    if (!team_mode) return has_killed_token(player);
    
    for (int i = 0; i < 2; i++) {
        if (teams[i].player1_id == player->id || teams[i].player2_id == player->id) {
            int teammate_id = (teams[i].player1_id == player->id) ? 
                             teams[i].player2_id : teams[i].player1_id;
            return (player->hit_record > 0 || players[teammate_id].hit_record > 0);
        }
    }
    return false;
}

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
    
    // Initialize original safe squares
    board[6][1] = 'S';
    board[1][8] = 'S';
    board[8][13] = 'S';
    board[13][6] = 'S';
    
    // Add colored safe paths
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
        players[i].hit_record = 0;
        players[i].home_tokens = 0;
        players[i].is_active = true;
        players[i].num_tokens = num_tokens_per_player;
        players[i].rank = 0;
        
        for(int j = 0; j < num_tokens_per_player; j++) {
            players[i].token_positions[j] = -1;
            
            switch(i) {
                case 0:
                    players[i].tokens[j][0] = 2 + (j/2);
                    players[i].tokens[j][1] = 2 + (j%2);
                    break;
                case 1:
                    players[i].tokens[j][0] = 2 + (j/2);
                    players[i].tokens[j][1] = 11 + (j%2);
                    break;
                case 2:
                    players[i].tokens[j][0] = 11 + (j/2);
                    players[i].tokens[j][1] = 11 + (j%2);
                    break;
                case 3:
                    players[i].tokens[j][0] = 11 + (j/2);
                    players[i].tokens[j][1] = 2 + (j%2);
                    break;
            }
        }
    }
}

void display_board() {
    
    pthread_mutex_lock(&board_mutex);
    system("clear");
    
    printf("\n  ");
    for(int j = 0; j < BOARD_SIZE; j++) {
       // printf(" %2d", j);
    }
    printf("\n");
    
    for(int i = 0; i < BOARD_SIZE; i++) {
        //printf("%2d ", i);
        for(int j = 0; j < BOARD_SIZE; j++) {
            bool token_present = false;
            for(int p = 0; p < NUM_PLAYERS; p++) {
                for(int t = 0; t < players[p].num_tokens; t++) {
                    if(players[p].tokens[t][0] == i && players[p].tokens[t][1] == j) {
                        printf("\033[1;%dm%c \033[0m", 
                        (p == 0) ? 31 :  // Red
                        (p == 1) ? 33 :  // Yellow
                        (p == 2) ? 32 :  // Green
                        34,              // Blue
                        players[p].symbol);
                        token_present = true;
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
                    case 'r': printf("\033[1;31m• \033[0m"); break;  // Red dot
                    case 'y': printf("\033[1;33m• \033[0m"); break;  // Yellow dot
                    case 'g': printf("\033[1;32m• \033[0m"); break;  // Green dot
                    case 'b': printf("\033[1;34m• \033[0m"); break;  // Blue dot
                    case 'S': printf("\033[1;37m* \033[0m"); break;
                    default: printf("□ ");
                }
            }
        }
        printf("\n");
    }
    
    pthread_mutex_unlock(&board_mutex);
}

int roll_dice() {
    pthread_mutex_lock(&dice_mutex);
    int result = (rand() % 6) + 1;
    pthread_mutex_unlock(&dice_mutex);
    return result;
}

bool is_safe_square(int row, int col) {
    if(board[row][col] == 'S') return true;
    
    for(int p = 0; p < NUM_PLAYERS; p++) {
        for(int t = 0; t < players[p].num_tokens; t++) {
            if(players[p].tokens[t][0] == row && players[p].tokens[t][1] == col) {
                return false;
            }
        }
    }
    return true;
}

bool can_enter_home(Player* player) {
    return player->hit_record > 0;
}

bool is_token_home(Player* player, int token_idx) {
    int pos = player->token_positions[token_idx];
    return pos >= PATH_LENGTH;
}

bool can_move_token(Player* player, int token_idx, int steps) {
    if(is_token_home(player, token_idx)) return false;
    
    int curr_pos = player->token_positions[token_idx];
    int new_pos = curr_pos + steps;
    
    // If token would move beyond PATH_LENGTH
    if(new_pos >= PATH_LENGTH) {
        if(team_mode) {
            // In team mode, check if any team member has killed
            if(!team_has_killed(player)) {
                new_pos = new_pos % PATH_LENGTH;  // Loop back if no team kills
            } else {
                return new_pos == PATH_LENGTH;  // Can enter home if team has kills
            }
        } else {
            // Single player mode logic remains the same
            if(!has_killed_token(player)) {
                new_pos = new_pos % PATH_LENGTH;
            } else {
                return new_pos == PATH_LENGTH;
            }
        }
    }
    
    // Get coordinates for new position
    int new_row = path_coords[player->id][new_pos][0];
    int new_col = path_coords[player->id][new_pos][1];
    
    // Check for central path access
    bool entering_central_path = (new_row == 7 || new_col == 7) && 
                               (curr_pos == -1 || path_coords[player->id][curr_pos][0] != 7 && 
                                path_coords[player->id][curr_pos][1] != 7);
    
    if(team_mode && entering_central_path && !team_has_killed(player)) {
        return false;  // Team needs at least one kill to enter central path
    } else if(!team_mode && entering_central_path && !has_killed_token(player)) {
        return false;  // Individual needs kill in single player mode
    }
    
    // Rest of the function remains the same
    if(board[new_row][new_col] == 'S') {
        for(int p = 0; p < NUM_PLAYERS; p++) {
            for(int t = 0; t < players[p].num_tokens; t++) {
                if(players[p].tokens[t][0] == new_row && 
                   players[p].tokens[t][1] == new_col && 
                   players[p].token_positions[t] >= 0) {
                    return false;
                }
            }
        }
    }
    
    return true;
}



void check_hits(Player* player, int new_row, int new_col) {
    pthread_mutex_lock(&board_mutex);
    
    // First check if there's a team block
    if(team_mode) {
        int block_count = 0;
        int block_team = -1;
        
        for(int p = 0; p < NUM_PLAYERS; p++) {
            if(p == player->id) continue;
            
            for(int t = 0; t < players[p].num_tokens; t++) {
                if(players[p].tokens[t][0] == new_row && 
                   players[p].tokens[t][1] == new_col) {
                    if(block_count == 0) {
                        for(int i = 0; i < 2; i++) {
                            if(teams[i].player1_id == p || teams[i].player2_id == p) {
                                block_team = i;
                                break;
                            }
                        }
                        block_count++;
                    } else if(block_team != -1) {
                        // Check if second piece belongs to same team
                        if((teams[block_team].player1_id == p || teams[block_team].player2_id == p) &&
                           !are_teammates(player->id, p)) {
                            // This is a team block, cannot move here
                            pthread_mutex_unlock(&board_mutex);
                            return;
                        }
                    }
                }
            }
        }
    }
    
    // Regular hit check
    for(int p = 0; p < NUM_PLAYERS; p++) {
        if(p == player->id || (team_mode && are_teammates(player->id, p))) continue;
        
        for(int t = 0; t < players[p].num_tokens; t++) {
            if(players[p].tokens[t][0] == new_row && 
               players[p].tokens[t][1] == new_col && 
               players[p].token_positions[t] >= 0 &&
               !is_safe_square(new_row, new_col)) {
                
                players[p].tokens[t][0] = players[p].tokens[t][0];
                players[p].tokens[t][1] = players[p].tokens[t][1];
                players[p].token_positions[t] = -1;
                
                player->hit_record++;
                printf("\n\033[1;37m[HIT]\033[0m Player %s hit Player %s's token %d!\n", 
                       player->color, players[p].color, t + 1);
                pthread_mutex_unlock(&board_mutex);
                return;
            }
        }
    }
    pthread_mutex_unlock(&board_mutex);
}


void move_token(Player* player, int token_idx, int steps) {
    int curr_pos = player->token_positions[token_idx];
    int new_pos = curr_pos + steps;
    
    // Handle movement beyond PATH_LENGTH
    if(new_pos >= PATH_LENGTH) {
        if(team_mode || has_killed_token(player)) {
            // Original behavior for team mode or after getting a kill
            if(team_mode || can_enter_home(player)) {
                player->home_tokens++;
                player->token_positions[token_idx] = PATH_LENGTH;
                player->tokens[token_idx][0] = -1;  // Remove token from board
                player->tokens[token_idx][1] = -1;
                printf("\n\033[1;32m[HOME]\033[0m Player %s's token %d reached home!\n", 
                       player->color, token_idx + 1);
                return;
            }
            return;
        } else {
            // Loop back to start if no kill yet
            new_pos = new_pos % PATH_LENGTH;
            printf("\n\033[1;33m[LOOP]\033[0m Player %s's token %d loops back to continue hunting!\n", 
                   player->color, token_idx + 1);
        }
    }
    
    int new_row = path_coords[player->id][new_pos][0];
    int new_col = path_coords[player->id][new_pos][1];
    
    check_hits(player, new_row, new_col);
    
    player->tokens[token_idx][0] = new_row;
    player->tokens[token_idx][1] = new_col;
    player->token_positions[token_idx] = new_pos;
}

bool check_win(Player* player) {
    if(!team_mode) {
        return player->home_tokens == player->num_tokens;
    }
    
    // In team mode, check if both players in the team have finished
    if(player->home_tokens == player->num_tokens) {
        for(int i = 0; i < 2; i++) {
            if(teams[i].player1_id == player->id || teams[i].player2_id == player->id) {
                int teammate_id = (teams[i].player1_id == player->id) ? 
                                 teams[i].player2_id : teams[i].player1_id;
                
                // Only count as a win if both players have finished
                if(players[teammate_id].home_tokens == players[teammate_id].num_tokens && 
                   player->home_tokens == player->num_tokens) {
                    // Set both players as inactive
                    player->is_active = false;
                    players[teammate_id].is_active = false;
                    
                    // Assign ranks if not already assigned
                    if(player->rank == 0) player->rank = current_rank;
                    if(players[teammate_id].rank == 0) players[teammate_id].rank = current_rank;
                    
                    // Both players on opposing team get the next rank
                    int opposing_team = (i + 1) % 2;
                    int opp1_id = teams[opposing_team].player1_id;
                    int opp2_id = teams[opposing_team].player2_id;
                    
                    // Only assign ranks to opposing team if they haven't finished yet
                    if(players[opp1_id].rank == 0) players[opp1_id].rank = current_rank + 1;
                    if(players[opp2_id].rank == 0) players[opp2_id].rank = current_rank + 1;
                    
                    // End the game only when both players in a team have finished
                    active_players = 1; // This will trigger game end
                    printf("\nTeam %d (%s & %s) has won the game!\n", 
                           i + 1, players[player->id].color, players[teammate_id].color);
                    return true;
                } else if(player->home_tokens == player->num_tokens) {
                    // If only this player has finished, mark them as inactive but don't end game
                    player->is_active = false;
                    printf("\nPlayer %s has finished! Waiting for teammate %s to finish...\n",
                           player->color, players[teammate_id].color);
                    active_players--;
                    return false;
                }
                break;
            }
        }
    }
    return false;
}


// Modify player_turn function to handle game termination
void* player_turn(void* arg) {
    Player* player = (Player*)arg;
    int consecutive_unable_to_move = 0;
    int consecutive_sixes = 0;  // Track number of consecutive 6s
    bool teammate_finished = false;
    int teammate_id = -1;

    if(team_mode) {
        for(int i = 0; i < 2; i++) {
            if(teams[i].player1_id == player->id) {
                teammate_id = teams[i].player2_id;
                break;
            } else if(teams[i].player2_id == player->id) {
                teammate_id = teams[i].player1_id;
                break;
            }
        }
    }
    
    while(player->is_active && active_players > 1) {
        sem_wait(&dice_semaphore);
        
        pthread_mutex_lock(&turn_mutex);
        while(previous_turn == player->id) {
            pthread_cond_wait(&turn_cond, &turn_mutex);
        }
        
        bool continue_turn = true;
        while(continue_turn && player->is_active) {
            int dice_value = roll_dice();
            printf("\nPlayer %s rolled: %d\n", player->color, dice_value);
            
            if(dice_value == 6) {
                consecutive_sixes++;
                if(consecutive_sixes == 3) {
                    printf("Third consecutive 6! Turn forfeited for Player %s\n", player->color);
                    consecutive_sixes = 0;
                    continue_turn = false;
                    sem_post(&dice_semaphore);
                    sem_wait(&board_semaphore);
                    display_board();
                    sem_post(&board_semaphore);
                    break;
                }
            } else {
                consecutive_sixes = 0;
                continue_turn = false;
            }
            
            sem_post(&dice_semaphore);
            sem_wait(&board_semaphore);
            
            bool moved = false;
            
            // Check if player can move their own pieces
            for(int i = 0; i < player->num_tokens; i++) {
                if(player->token_positions[i] == -1 && dice_value == 6) {
                    player->token_positions[i] = 0;
                    player->tokens[i][0] = path_coords[player->id][0][0];
                    player->tokens[i][1] = path_coords[player->id][0][1];
                    moved = true;
                    printf("Player %s started a new token\n", player->color);
                    consecutive_unable_to_move = 0;
                    break;
                } else if(player->token_positions[i] >= 0 && can_move_token(player, i, dice_value)) {
                    move_token(player, i, dice_value);
                    moved = true;
                    printf("Player %s moved token %d\n", player->color, i + 1);
                    consecutive_unable_to_move = 0;
                    break;
                }
            }
            
            // If player has finished and rolled a 6, they can move teammate's pieces
            if(team_mode && player->home_tokens == player->num_tokens && dice_value == 6) {
                Player* teammate = NULL;
                for(int i = 0; i < 2; i++) {
                    if(teams[i].player1_id == player->id) {
                        teammate = &players[teams[i].player2_id];
                        break;
                    } else if(teams[i].player2_id == player->id) {
                        teammate = &players[teams[i].player1_id];
                        break;
                    }
                }
                
                if(teammate && !teammate->is_active) {
                    dice_value = roll_dice();  // Roll again for teammate
                    printf("\nPlayer %s rolling for teammate %s: %d\n", 
                           player->color, teammate->color, dice_value);
                    
                    for(int i = 0; i < teammate->num_tokens; i++) {
                        if(can_move_token(teammate, i, dice_value)) {
                            move_token(teammate, i, dice_value);
                            moved = true;
                            printf("Player %s moved teammate %s's token %d\n", 
                                   player->color, teammate->color, i + 1);
                            break;
                        }
                    }
                }
            }
            
            if(!moved) {
                printf("Player %s couldn't move any token\n", player->color);
                consecutive_unable_to_move++;
                
                if(consecutive_unable_to_move >= 10 && active_players <= 2) {
                    printf("\nPlayer %s is stuck and cannot proceed. Game over.\n", player->color);
                    player->is_active = false;
                    player->rank = current_rank++;
                    active_players--;
                }
            }
            
            display_board();
            
            if(check_win(player)) {
            if(team_mode && teammate_id != -1) {
                if(players[teammate_id].home_tokens == players[teammate_id].num_tokens) {
                    // Both players have finished, end the game
                    player->is_active = false;
                    player->rank = current_rank++;
                    active_players--;
                    printf("\nBoth players in the team have finished!\n");
                    continue_turn = false;
                }
            } else {
                // Non-team mode or single player finished
                player->is_active = false;
                player->rank = current_rank++;
                active_players--;
                continue_turn = false;
            }
        }
            
            sem_post(&board_semaphore);
            
            // If it's not a 6 or player couldn't move, end turn
            if(!moved || dice_value != 6) {
                continue_turn = false;
            } else {
                printf("\nPlayer %s gets another turn for rolling a 6!\n", player->color);
            }
            
            usleep(500000);
        }
        
        previous_turn = player->id;
        pthread_cond_broadcast(&turn_cond);
        pthread_mutex_unlock(&turn_mutex);
        
        usleep(500000);
    }
    
    return NULL;
}



int main() {
    srand(time(NULL));
    
    sem_init(&dice_semaphore, 0, 1);
    sem_init(&board_semaphore, 0, 1);
    
    initialize_board();
    initialize_players();
    initialize_teams();
    
    printf("\nInitial Ludo Board State:\n");
    display_board();
    
    printf("\nBoard Legend:\n");
    printf("\033[1;31m█ \033[0m- Red Home\n");
    printf("\033[1;33m█ \033[0m- Yellow Home\n");
    printf("\033[1;32m█ \033[0m- Green Home\n");
    printf("\033[1;34m█ \033[0m- Blue Home\n");
    printf("\033[1;37m* \033[0m- Safe Square\n");
    printf("□ - Path\n");
    
    printf("\nStarting game with %d tokens per player\n", num_tokens_per_player);
    
    // Create player threads with random order
    int thread_order[NUM_PLAYERS] = {0, 1, 2, 3};
    for(int i = NUM_PLAYERS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = thread_order[i];
        thread_order[i] = thread_order[j];
        thread_order[j] = temp;
    }
    
    printf("\nPlayer order: ");
    for(int i = 0; i < NUM_PLAYERS; i++) {
        printf("%s ", players[thread_order[i]].color);
        pthread_create(&players[thread_order[i]].thread_id, NULL, 
                      player_turn, &players[thread_order[i]]);
    }
    printf("\n");
    
    // Wait for game to complete
    while(active_players > 1) {
        usleep(100000);
    }
    
    // Display final results
    printf("\n=== Game Over ===\n");
    
    if (team_mode) {
        TeamResult team_results[2];
        
        // Initialize team results
        for (int i = 0; i < 2; i++) {
            team_results[i].team_number = i + 1;
            strcpy(team_results[i].player1_color, players[teams[i].player1_id].color);
            strcpy(team_results[i].player2_color, players[teams[i].player2_id].color);
            team_results[i].total_hits = players[teams[i].player1_id].hit_record + 
                                       players[teams[i].player2_id].hit_record;
            
            // Calculate team rank based on player ranks
            int player1_rank = players[teams[i].player1_id].rank;
            int player2_rank = players[teams[i].player2_id].rank;
            team_results[i].rank = (player1_rank < player2_rank) ? player1_rank : player2_rank;
        }
        
        // Sort teams by rank
        if (team_results[1].rank < team_results[0].rank) {
            TeamResult temp = team_results[0];
            team_results[0] = team_results[1];
            team_results[1] = temp;
        }
        
        printf("\nTeam Results:\n");
        printf("\033[1;32m🏆 WINNING TEAM: Team %d (%s & %s)\033[0m\n", 
               team_results[0].team_number,
               team_results[0].player1_color,
               team_results[0].player2_color);
        
        for (int i = 0; i < 2; i++) {
            printf("\nTeam %d (%s & %s):\n", 
                   team_results[i].team_number,
                   team_results[i].player1_color,
                   team_results[i].player2_color);
            printf("  Total Hits: %d\n", team_results[i].total_hits);
            printf("  Final Rank: %d\n", team_results[i].rank);
            
            // Show individual player stats
            printf("  Individual Stats:\n");
            int p1_id = teams[i].player1_id;
            int p2_id = teams[i].player2_id;
            printf("    %s: %d hits (Rank: %d)\n", 
                   players[p1_id].color, 
                   players[p1_id].hit_record,
                   players[p1_id].rank);
            printf("    %s: %d hits (Rank: %d)\n", 
                   players[p2_id].color, 
                   players[p2_id].hit_record,
                   players[p2_id].rank);
        }
    } else {
        printf("\nFinal Rankings:\n");
        for(int rank = 1; rank <= NUM_PLAYERS; rank++) {
            for(int i = 0; i < NUM_PLAYERS; i++) {
                if(players[i].rank == rank) {
                    printf("%d. %s (Hit rate: %d)\n", 
                           rank, players[i].color, players[i].hit_record);
                    break;
                }
            }
        }
    }
    
    printf("\nCancelling remaining threads...\n");
    for(int i = 0; i < NUM_PLAYERS; i++) {
        if(players[i].is_active) {
            pthread_cancel(players[i].thread_id);
            printf("Cancelled thread for player %s (ID: %lu)\n", 
                   players[i].color, (unsigned long)players[i].thread_id);
        }
    }
    
    // Clean up resources
    sem_destroy(&dice_semaphore);
    sem_destroy(&board_semaphore);
    pthread_mutex_destroy(&board_mutex);
    pthread_mutex_destroy(&dice_mutex);
    pthread_mutex_destroy(&turn_mutex);
    pthread_cond_destroy(&turn_cond);
    
    return 0;
}