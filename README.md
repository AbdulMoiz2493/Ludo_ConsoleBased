# Ludo Board Game (Multithreaded Implementation in C)

## Introduction
Ludo is a strategy board game for two to four players, where players race their four tokens from start to finish based on rolls of a single die. This project is a multithreaded implementation of Ludo in C, ensuring synchronization using threads, mutexes, and semaphores.

## Features
- Four-player Ludo game with up to four tokens per player.
- Randomized turn order instead of sequential turns.
- Multi-threading using Pthreads to manage players and the game state.
- Synchronization mechanisms to handle shared resources (board and dice).
- Safe squares and hitting rules implemented.
- Special rules such as three consecutive sixes leading to a lost turn.
- Master thread to manage player eliminations and game progress.
- Team mode with shared rolls after a player completes their tokens.

## Implementation Details
### **Phase 1: Board Creation and Resource Management**
- Implement the Ludo board as a grid.
- Initialize player tokens in their respective home yards.
- Define global resources: the Ludo grid and the dice.
- Ensure mutual exclusion using mutexes when accessing shared resources.
- Expected Output:
  - The complete Ludo board with player tokens in home yards.

### **Phase 2: Full Game Implementation**
- Implement token movement using semaphores.
- Accept user input for the number of tokens (1 to 4 per player).
- Ensure each player gets a random turn within an iteration.
- Implement mutual exclusion when accessing the dice and board resources.
- Manage hit conditions and safe squares.
- Implement player elimination based on a lack of sixes or hits over 20 turns.
- Allow players to form teams and share rolls upon completion.
- Expected Output:
  - Dynamic Ludo board display after each iteration.
  - Player rankings (1st, 2nd, 3rd place).
  - Canceled threads with player details.
  - Number of tokens per player.
  - Hit rate tracking.

## Technical Details
### **Threading Strategy**
- Four threads for four players.
- A master thread to manage eliminations and the game state.
- Additional threads to handle row/column checks and token movements.

### **Synchronization Mechanisms**
- Mutex locks to ensure exclusive access to dice and board.
- Semaphores to manage token movements.
- Conditional variables to maintain turn order integrity.

### **Thread Communication**
- Use Pthreads to create worker threads with parameter passing via structures.
- The master thread tracks game progress and cancels threads as needed.

## Compilation and Execution
1. Compile the program:
   ```bash
   gcc -pthread ludo.c -o ludo
   ```
2. Run the program:
   ```bash
   ./ludo
   ```
3. Follow on-screen prompts to enter the number of tokens and play.

## Future Enhancements
- Graphical User Interface (GUI) integration.
- Online multiplayer support.
- AI-based opponents for single-player mode.

---

## Contact

- **Abdul Moiz**  
- Email: abdulmoiz8895@gmail.com 
- GitHub: [AbdulMoiz2493](https://github.com/AbdulMoiz2493)

