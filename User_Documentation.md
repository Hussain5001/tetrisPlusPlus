# User Documentation

## Introduction
This is an introduction guide to our game of Tetris. It is a classic puzzle video game that involves fitting falling geometric shapes (Tetriminos) into a grid to create complete rows, which then disappear.

## On building the code
After you've successfully built the code, a Main Menu opens that gives you three play options (use the mouse, the arrow keys + Enter, or hand gestures to choose): 
1. **Zen Mode** : This is a non-timed version where user can play for as long as they want until the game finishes. Before playing, a setup screen lets them choose the **start level** (1-10, which sets the speed and score multiplier), the **pace** (how many lines until the next level: chill 10, steady 8, rising 5, brutal 3), the phosphor colour, the ghost piece, the CRT effect and sound. From the pause menu (Esc, P, or hold a fist) they can **Quit** the game or **Save & Quit** if they'd like to save their progress, and continue later with **Continue Saved**.
2. **Time Attack**: This is a 120 seconds timed game wherein the user has to score as much as they can in 120 seconds.
3. **First Forty Lines Clearance**: In this, the user has to clear 40 lines as fast as they can. along with the timer, you get a **Lines** prompter which tells you the number of lines cleared so far. 

In all the modes, the pause menu also lets the user go back to the **Main Menu** in case they change their mind about the mode they want to play. When a game ends, **Play Again** starts the same mode again.

## Controls
The game is pretty intuitive, wherein the tetrominoes can be controlled as follows: 
* Right arrow key: move block right (hold to keep moving)
* Left arrow key: move block left (hold to keep moving)
* Up arrow key or X: rotate the block
* Down arrow key: move block one row down
* Spacebar: force the block to fall directly.
* Esc or P: pause

The faint `[]` outline at the bottom of the board (the "ghost") shows where the block will land. A block that lands can still be slid or rotated for half a second before it locks, and rotating next to a wall or another block nudges it into place (a "wall kick").
* M: sound on/off, F11: fullscreen

### Hands-free controls
Run `./build/Tetris --gestures` (after `pip install -r gesture/requirements.txt`) to play with your webcam:
Point at the camera with your index finger (other fingers curled in):
* Flick the finger left / right: move the block one column
* Flick the pointing hand down: force fall
* Pinch (thumb to index fingertip): rotate
* Hold a fist: pause / select in menus
* Point mode (`--mode point`): tilt the finger and hold it to keep moving
* Palm mode (`--mode palm`): the original open-hand swipes
* Position mode (`--mode position`): the block follows your hand sideways
* Press `m` in the camera window to switch modes

See the README for tuning tips.

## More on the 3 Modes

### Zen Mode:

This is the classic tetris mode where you can play till the entire grid fills up. The speed of the falling Tetrominos increases every time you level up; how many lines that takes depends on the pace chosen on the setup screen (5 lines on "rising", like the original game).
#### Game options:
Will have 2 options to choose from either start a new game or load a saved game.
#### Game end:
The game will run till the grid fills up and there is no space for the new block to spawn
#### Scoring:
There is a score_multiplier(SM) of 1 + 0.5 for every level above 1, so a higher start level and every level up are worth more.
* +SM ; as the block descends each row on its own
* +1 ; for every row you move the block down with the down key
* +20; for using force fall (SpaceBar)
* +300*SM; for every line cleared
* +500*SM; for every line cleared if 4 lines cleared together

### Time Attack:
The user gets 120 seconds to score as much as they can. The timer stops while the game is paused.



#### Game end:
If 120 seconds are over or grid fills up and there is no space for the new block to spawn

#### Scoring:
* +20; for using force fall
* +1; for every row moved down with the down key
* +200; if one line cleared
* +300; per line if 2 lines cleared together
* +400; per line if 3 lines cleared together
* +600; per line if 4 lines cleared together


### First forty lines cleared mode:
Aim is to clear the 40 lines as fast as possible

#### Game end: 
The game ends when 40 lines cleared or grid fills up and there is no space for the new block to spawn.

#### Scoring:
* Time taken is itself the score

## High scores
The best five results of each mode and your most recent game are saved in `scores.json`. The menu shows the best and last result for the selected mode, and the game-over screen shows where your result ranks. In First 40 Lines, only finished runs (40 lines cleared) count, and faster is better.
