#include "board.h"
#include <iostream>
#include "raylib.h"
#include "ui/Draw.h"

// constructor
Board::Board(int row,int col):num_rows{row},num_cols{col}{

// displays when grid is successfully constructed
  std::cout<<"constructed grid"<<std::endl;

  //set values
  cell_size=30;

  //allocate space for grid
  create_grid();
  //initialize grid with zeros
  clear_grid();

}

// destructor
 Board::~Board() {
  // displays state of grid before destroying it
   show_state();
   // displays message when grid is succesfully destroyed
   std::cout<<"destroyed grid"<<std::endl;
   // deallocate memory for the grid
   for (int i = 0; i < num_rows; ++i) {
     delete[] grid[i];
   }
   delete[] grid;
 }

// creates a grid with the specified number of rows and columns
 void Board::create_grid() {
   grid = new int*[num_rows];
   for (int i = 0; i < num_rows; i++) {
     grid[i] = new int[num_cols];
   }
 }

// clears the grid to all values 0
void Board::clear_grid() {
  for (int i = 0; i < num_rows; i++) {
    for (int j = 0; j < num_cols; j++) {
      grid[i][j] = 0;
    }
  }
}

// displays the current state of the grid
void Board::show_state() {
  for(int r=0;r<num_rows;r++){
    for(int c=0;c<num_cols;c++){
      std::cout<<grid[r][c];
    }
    std::cout<<std::endl;
  }
}

// a function that checks is a row is full
bool Board::is_row_full(int row) {
    for (int j = 0; j < num_cols; j++) {
        if (grid[row][j] == 0) {
            return false;
        }
    }
    return true;
}


// to check if the cell is within bounds 
bool Board::is_cell_within_bounds(int row, int col){
  if(row>=0 && col>=0 && row<num_rows && col<num_cols){
    return false;
  }
  return true;
}

// a function to draw the grid: a dark well with faint grid lines and
// bevelled blocks for the filled cells
void Board::draw(int x, int y){
  int width=num_cols*cell_size;
  int height=num_rows*cell_size;
  DrawRectangle(x,y,width,height,cell_color(0));
  for(int c=1;c<num_cols;c++){
    DrawLine(x+c*cell_size,y,x+c*cell_size,y+height,ColorAlpha(WHITE,0.04f));
  }
  for(int r=1;r<num_rows;r++){
    DrawLine(x,y+r*cell_size,x+width,y+r*cell_size,ColorAlpha(WHITE,0.04f));
  }
  for(int r=0;r<num_rows;r++){
    for(int c=0;c<num_cols;c++){
      if(grid[r][c]!=0){
        ui::draw_cell(x+c*cell_size,y+r*cell_size,cell_size,cell_color(grid[r][c]));
      }
    }
  }
}

int Board::get_cell_size(){
  return cell_size;
}

int Board::get_num_cols(){
  return num_cols;
}

// to check if the cell is empty
bool Board::is_cell_empty(int row, int col) {
    return grid[row][col] == 0;
}
// a function to clear a row and shift the above row down  
int Board::row_clearance(){
  int full_row=0;
  last_cleared_rows.clear();
  for(int r=num_rows-1;r>=0;r--){
    if(is_row_full(r)){
      last_cleared_rows.push_back(r);
      for(int c=0;c<num_cols;c++){
        grid[r][c]=0;
      }
      full_row++;
    }else if(full_row>0){
         for(int col=0;col<num_cols;col++){
           grid[r+full_row][col]=grid[r][col];
           grid[r][col]=0;
         }
       }
  }
  if(full_row>0){
    clear_events++;
  }
  return full_row;
}

//Getter function for returning the number of rows
int Board::get_num_rows(){
  return num_rows;
}
//Getter function for returning the grid_value
int Board::get_grid_value(int given_row,int given_column){
  return grid[given_row][given_column];
}

