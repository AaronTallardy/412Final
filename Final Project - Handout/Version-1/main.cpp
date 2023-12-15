//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2020-12-01, rev. 2023-12-04
//
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.

#include <iostream>
#include <string>
#include <random>
//
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <algorithm>


#include <chrono>
#include <thread>
//
#include "gl_frontEnd.h"

//	feel free to "un-use" std if this is against your beliefs.
using namespace std;

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Private Functions' Prototypes
//-----------------------------------------------------------------------------
#endif

void initializeApplication(void);
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = Direction::NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd);
void generateWalls(void);
void generatePartitions(void);
void moveTraveler(int travelerIndex);
void atExit(int travelerIndex);
Direction decideDirection(Direction forbiddenDir1, Direction forbiddenDir2, int travelerIndex);
void createThreads(void);
void threadFunc(int threadIndex);

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Application-level Global Variables
//-----------------------------------------------------------------------------
#endif

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType** grid;
unsigned int numRows = 0;	//	height of the grid
unsigned int numCols = 0;	//	width
unsigned int numTravelers = 0;	//	initial number
unsigned int numTravelersDone = 0;
unsigned int numLiveThreads = 0;		//	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList;
GridPosition	exitPos;	//	location of the exit
unsigned int snakeGrowSpeed = 0;

//	travelers' sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 1000;
int travelerSleepTime = 100000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
time_t launchTime;

//	Random generators:  For uniform distributions
const unsigned int MAX_NUM_INITIAL_SEGMENTS = 6;
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> unsignedNumberGenerator(0, numeric_limits<unsigned int>::max());
uniform_int_distribution<unsigned int> segmentNumberGenerator(0, MAX_NUM_INITIAL_SEGMENTS);
uniform_int_distribution<unsigned int> segmentDirectionGenerator(0, static_cast<unsigned int>(Direction::NUM_DIRECTIONS)-1);
uniform_int_distribution<unsigned int> headsOrTails(0, 1);
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;

//Tracks if game is over
bool gamestate = false;

std::vector<std::thread> threadVec;

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Functions called by the front end
//-----------------------------------------------------------------------------
#endif
//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void drawTravelers(void)
{
	//-----------------------------
	//	You may have to sychronize things here
	//-----------------------------
	for (unsigned int k=0; k<travelerList.size(); k++)
	{
		atExit(k); 
		//	here I would test if the traveler thread is still live
		if(travelerList[k].isAlive)
		{	
			drawTraveler(travelerList[k]);
		}
		
	}
}

void updateMessages(void)
{
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	unsigned int numMessages = 4;
	sprintf(message[0], "We created %d travelers", numTravelers);
	sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
	sprintf(message[2], "I like cheese");
	sprintf(message[3], "Simulation run time: %ld s", time(NULL)-launchTime);
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//---------------------------------------------------------
	drawMessages(numMessages, message);
}

void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;

	switch (c)
	{
		//	'esc' to quit
		case 27:
			gamestate = true;
			for(int i = 0; i < threadVec.size(); i++){
			threadVec[i].join();
			}
			exit(0);
			break;

		//	slowdown
		case ',':
			slowdownTravelers();
			ok = 1;
			break;

		//	speedup
		case '.':
			speedupTravelers();
			ok = 1;
			break;

		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}


//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupTravelers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers(void)
{
	//	increase sleep time by 20%.  No upper limit on sleep time.
	//	We can slow everything down to admistrative pace if we want.
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}




//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.
	//	So far, I hard code-some values
	numRows = atoi(argv[1]);
	numCols = atoi(argv[2]);
	numTravelers = 1;
	numLiveThreads = 0;
	numTravelersDone = 0;

	if (argc > 4)
	{
		snakeGrowSpeed = atoi(argv[4]);
	}
	numLiveThreads = numTravelers;
	numTravelersDone = 0;

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv);
	
	//	Now we can do application-level initialization
	initializeApplication();

	launchTime = time(NULL);
	gamestate = false;
	createThreads();

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (unsigned int i=0; i< numRows; i++)
		delete []grid[i];
	delete []grid;
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		delete []message[k];
	delete []message;
	
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}

/**
 *	@brief The function to process each segment list on their own thread
 *	@param threadIndex The index of the thread stored in a vector
 *	@return void
 */
void threadFunc(int threadIndex)
{
	while(!gamestate)
	{
		moveTraveler(threadIndex);
		std::this_thread::sleep_for(std::chrono::milliseconds(travelerSleepTime / 1000));
	}
	
}

/**
 *	@brief The function that creates each thread
 *	@param void
 *	@return void
 */
void createThreads(void)
{
	for(int i = 0; i < numTravelers; i++)
	{
		threadVec.push_back(std::thread(threadFunc, i));
	}
}


//==================================================================================
//
//	This is a function that you have to edit and add to.
//
//==================================================================================


void initializeApplication(void)
{
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);

	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (unsigned int i=0; i<numRows; i++)
	{
		grid[i] = new SquareType[numCols];
		for (unsigned int j=0; j< numCols; j++)
			grid[i][j] = SquareType::FREE_SQUARE;
		
	}

	message = new char*[MAX_NUM_MESSAGES];
	for (unsigned int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
		
	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator after ranting in class that the C random
	//	generator was junk.  Here I am not using it to produce "serious" data (as in a
	//	real simulation), only wall/partition location and some color
	srand((unsigned int) time(NULL));

	//	generate a random exit
	exitPos = getNewFreePosition();
	grid[exitPos.row][exitPos.col] = SquareType::EXIT;

	//	Generate walls and partitions
	generateWalls();
	generatePartitions();
	
	//	Initialize traveler info structs
	//	You will probably need to replace/complete this as you add thread-related data
	float** travelerColor = createTravelerColors(numTravelers);
	for (unsigned int k=0; k<numTravelers; k++) {
		GridPosition pos = getNewFreePosition();
		//	Note that treating an enum as a sort of integer is increasingly
		//	frowned upon, as C++ versions progress
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		Traveler traveler;
		traveler.segmentList.push_back(seg);
		grid[pos.row][pos.col] = SquareType::TRAVELER;

        //    I add 0-n segments to my travelers
        unsigned int numAddSegments = segmentNumberGenerator(engine);
		TravelerSegment currSeg = traveler.segmentList[0];
		bool canAddSegment = true;
		// cout << "Traveler " << k << " at (row=" << pos.row << ", col=" << pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
		// cout << "\t";

		for (unsigned int s = 0; s < numAddSegments && canAddSegment; s++)
		{
			TravelerSegment newSeg = newTravelerSegment(currSeg, canAddSegment);
			if (canAddSegment)
			{
				traveler.segmentList.push_back(newSeg);
				currSeg = newSeg;
				cout << dirStr(newSeg.dir) << "  ";
			}
		}
		cout << endl;

		

		for (unsigned int c = 0; c < 4; c++)
			traveler.rgba[c] = travelerColor[k][c];

		travelerList.push_back(traveler);
	}

	slowdownTravelers();

	//	free array of colors
	for (unsigned int k = 0; k < numTravelers; k++)
		delete[] travelerColor[k];
	delete[] travelerColor;
}

//------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Generation Helper Functions
#endif
//------------------------------------------------------

GridPosition getNewFreePosition(void)
{
	GridPosition pos;

	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

Direction newDirection(Direction forbiddenDir)
{
	bool noDir = true;

	Direction dir = Direction::NUM_DIRECTIONS;
	while (noDir)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));
		noDir = (dir==forbiddenDir);
	}
	return dir;
}


/**
 *	@brief A function checking whether the current traveler is at the exit
 *	@param travelerIndex The index of the specific traveler segment
 *	@return void
 */
void atExit(int travelerIndex)
{
	//check if head is at the exit
	if(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT)
	{
		for(int i = 1; i < travelerList[travelerIndex].segmentList.size(); i++)
		{
			grid[travelerList[travelerIndex].segmentList[i].row][travelerList[travelerIndex].segmentList[i].col] = SquareType::FREE_SQUARE;
			
		}
		travelerList[travelerIndex].segmentList.clear();
		if(travelerList[travelerIndex].isAlive == true){
			numTravelersDone++;
		}
		travelerList[travelerIndex].isAlive = false;
		if (numTravelersDone == numTravelers){
			gamestate = true;
		}
	}
}

void moveNorth(int travelerIndex){
	if (travelerList[travelerIndex].segmentList[0].row > 0 &&
		(grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::FREE_SQUARE ||
		grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT))
	{
		//create a new head
		TravelerSegment newSeg;
		newSeg.row = travelerList[travelerIndex].segmentList[0].row - 1;
		newSeg.col = travelerList[travelerIndex].segmentList[0].col;
		newSeg.dir = Direction::NORTH;
		
		//push the new head to the front
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());
		travelerList[travelerIndex].segmentList.push_back(newSeg);
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());
	
		//check if snake needs to grow
		if(travelerList[travelerIndex].numMoves == snakeGrowSpeed)
		{
			travelerList[travelerIndex].numMoves = 0;
		}
		else
		{
			grid[travelerList[travelerIndex].segmentList.back().row][travelerList[travelerIndex].segmentList.back().col] = SquareType::FREE_SQUARE;
			travelerList[travelerIndex].segmentList.pop_back();
			travelerList[travelerIndex].numMoves++;
		}
		
	}
}

/**
 *	@brief The function that updates the travelers head position moving south
 *	@details If the number of moves is met, a tail will be added
 *	@param travelerIndex The index of the traveler segment
 *	@return void
 */
void moveSouth(int travelerIndex){
	if (travelerList[travelerIndex].segmentList[0].row + 1 <= numRows - 1 &&
		(grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::FREE_SQUARE || 
		grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT))
	{
		//create a new head 
		TravelerSegment newSeg;
		newSeg.row = travelerList[travelerIndex].segmentList[0].row + 1;
		newSeg.col = travelerList[travelerIndex].segmentList[0].col;
		newSeg.dir = Direction::SOUTH;
		
		//push the new head to the front
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());
		travelerList[travelerIndex].segmentList.push_back(newSeg);
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());

		//check if snake needs to grow
		if(travelerList[travelerIndex].numMoves == snakeGrowSpeed)
		{
			
			travelerList[travelerIndex].numMoves = 0;
		}
		else
		{
			grid[travelerList[travelerIndex].segmentList.back().row][travelerList[travelerIndex].segmentList.back().col] = SquareType::FREE_SQUARE;
			travelerList[travelerIndex].segmentList.pop_back();
			travelerList[travelerIndex].numMoves++;
		}
	}
}

/**
 *	@brief The function that updates the travelers head position moving east
 *	@details If the number of moves is met, a tail will be added
 *	@param travelerIndex The index of the traveler segment
 *	@return void
 */
void moveEast(int travelerIndex)
{
	if (travelerList[travelerIndex].segmentList[0].col + 1 <= numCols - 1 &&
	(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == SquareType::FREE_SQUARE ||
	grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == SquareType::EXIT))
	{
		//create a new head
		TravelerSegment newSeg;
		newSeg.row = travelerList[travelerIndex].segmentList[0].row;
		newSeg.col = travelerList[travelerIndex].segmentList[0].col + 1;
		newSeg.dir = Direction::EAST;
		
		//push the new head to the front
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());
		travelerList[travelerIndex].segmentList.push_back(newSeg);
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());

		//check if snake needs to grow
		if(travelerList[travelerIndex].numMoves == snakeGrowSpeed)
		{
			travelerList[travelerIndex].numMoves = 0;
		}
		else
		{
			grid[travelerList[travelerIndex].segmentList.back().row][travelerList[travelerIndex].segmentList.back().col] = SquareType::FREE_SQUARE;
			travelerList[travelerIndex].segmentList.pop_back();
			travelerList[travelerIndex].numMoves++;
		}
	}
	else
		travelerList[travelerIndex].segmentList[0].dir = decideDirection(Direction::EAST, Direction::WEST, travelerIndex);
}

/**
 *	@brief The function that updates the travelers head position moving West
 *	@details If the number of moves is met, a tail will be added
 *	@param travelerIndex The index of the traveler segment
 *	@return void
 */
void moveWest(int travelerIndex)
{
	if (travelerList[travelerIndex].segmentList[0].col > 0 &&
	(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == SquareType::FREE_SQUARE ||
	grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == SquareType::EXIT))
	{
		//create a new snake head
		TravelerSegment newSeg;
		newSeg.row = travelerList[travelerIndex].segmentList[0].row;
		newSeg.col = travelerList[travelerIndex].segmentList[0].col - 1;
		newSeg.dir = Direction::WEST;
		
		//push the new head to the front
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());
		travelerList[travelerIndex].segmentList.push_back(newSeg);
		std::reverse(travelerList[travelerIndex].segmentList.begin(), travelerList[travelerIndex].segmentList.end());

		//check if snake needs to grow
		if(travelerList[travelerIndex].numMoves == snakeGrowSpeed)
		{
			
			travelerList[travelerIndex].numMoves = 0;
		}
		else
		{
			grid[travelerList[travelerIndex].segmentList.back().row][travelerList[travelerIndex].segmentList.back().col] = SquareType::FREE_SQUARE;
			travelerList[travelerIndex].segmentList.pop_back();
			travelerList[travelerIndex].numMoves++;
		}
	}
}

/**
 * @brief move traveler to the new direction
 *
 * @param travelerIndex Index of the current traveler that we are moving
 */
void moveTraveler(int travelerIndex)
{
	// Move the traveler to the next position on the grid

	switch (travelerList[travelerIndex].segmentList[0].dir)
	{
	case Direction::NORTH:
	// Check if moving north is a valid move
		if (travelerList[travelerIndex].segmentList[0].row > 0 &&
			(grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::FREE_SQUARE ||
			grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT))
		{
			// If valid move traveler north
			moveNorth(travelerIndex);
			
		}
		
		else
		{
			// Else get a new direction
			Direction temp_direction = decideDirection(Direction::NORTH, Direction::SOUTH, travelerIndex);
			// Move to the new direction
			if(temp_direction == Direction::EAST){
				moveEast(travelerIndex);
				
			}
			else if(temp_direction == Direction::WEST)
			{
				moveWest(travelerIndex);
				
			}	
		}
			
		break;

	case Direction::SOUTH:
		// Check if moving south is a valid move
		if (travelerList[travelerIndex].segmentList[0].row + 1 <= numRows - 1 &&
			(grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::FREE_SQUARE || 
			grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT))
		{
			// If south is a valid move move south
			moveSouth(travelerIndex);
		}
		
		else
		{
			// Returns East or West
			Direction temp_direction = decideDirection(Direction::SOUTH, Direction::NORTH, travelerIndex);
			if(temp_direction == Direction::EAST)
			{
				moveEast(travelerIndex);
				
			}
			else if(temp_direction == Direction::WEST)
			{
				moveWest(travelerIndex);
				
			}
		}
		break;

	case Direction::WEST:
	// Check if west is a valid move
		if (travelerList[travelerIndex].segmentList[0].col > 0 &&
			(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == SquareType::FREE_SQUARE ||
			grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == SquareType::EXIT))
		{
			// move segment west
			moveWest(travelerIndex);
		}
		//	no more segment
		else
		{
			Direction temp_direction = decideDirection(Direction::WEST, Direction::EAST, travelerIndex);
			if(temp_direction == Direction::NORTH)
			{
				moveNorth(travelerIndex);
				//travelerList[travelerIndex].segmentList[1].dir = Direction::NORTH;
			}
			else if(temp_direction == Direction::SOUTH)
			{
				moveSouth(travelerIndex);
				//travelerList[travelerIndex].segmentList[1].dir = Direction::SOUTH;
			}
		}
		break;

	case Direction::EAST:
		// Check if east is a valid move
		if (travelerList[travelerIndex].segmentList[0].col + 1 <= numCols - 1 &&
			(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == SquareType::FREE_SQUARE ||
			grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == SquareType::EXIT))
		{
			moveEast(travelerIndex);
		}
		
		else
		{
			// Generate a new direction
			Direction temp_direction = decideDirection(Direction::EAST, Direction::WEST, travelerIndex);
			// Move to the new direction
			if(temp_direction == Direction::NORTH)
			{
				moveNorth(travelerIndex);
				
			}
			else if(temp_direction == Direction::SOUTH)
			{
				moveSouth(travelerIndex);
				
			}
		}
		break;

	default:
		return;
	}
	
	if(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col] != SquareType::EXIT)
	{
		grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col] = SquareType::TRAVELER;
	}

	
}

/**
 * @brief generate a new random direction based on the input parameters
 *
 *
 * @param forbiddenDir1 Original starting direction
 * @param forbiddenDir2 Inverse of the starting direction
 * @param travelerIndex Current traveler index used to check that the new direction is valid
 * @return Direction the newly generated direction or the starting direction depending on if the new direction is valid
 */
Direction decideDirection(Direction forbiddenDir1, Direction forbiddenDir2, int travelerIndex)
{
	bool noDir = true;
	// Generate a random new direction that is not the same as the previous directions
	Direction dir = Direction::NUM_DIRECTIONS;
	while (noDir == true)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		if(dir != forbiddenDir1 && dir != forbiddenDir2)
		{
			noDir = false;
		}
	}
	// Check the new direction
	switch(dir)
	{
		case Direction::NORTH:
		// Check if north of traveler is valid
			if (travelerList[travelerIndex].segmentList[0].row > 0 &&
			(grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::FREE_SQUARE ||
			grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT))
			{
				// If valid return the new direction
				return dir;
			}
			else
			{
				// else return current direction
				return forbiddenDir1;
			}
			break;
		case Direction::SOUTH:
		// Check if south of traveler is valid
			if (travelerList[travelerIndex].segmentList[0].row + 1 <= numRows - 1 &&
			(grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::FREE_SQUARE || 
			grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == SquareType::EXIT))
			{
				// If valid return the new direction
				return dir;
			}
			else
			{
				// else return current direction
				return forbiddenDir1;
			}
			break;
		case Direction::EAST:
		// Check if east of traveler is valid
			if (travelerList[travelerIndex].segmentList[0].col + 1 <= numCols - 1 &&
			(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == SquareType::FREE_SQUARE ||
			grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == SquareType::EXIT))
			{
				// If valid return the new direction
				return dir;
			}
			
			else
			{
				
				// else return current direction
				return forbiddenDir1;
			}
			break;
		case Direction::WEST:
		// Check if west of traveler is valid
			if (travelerList[travelerIndex].segmentList[0].col > 0 &&
			(grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == SquareType::FREE_SQUARE ||
			grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == SquareType::EXIT))
			{
				// If valid return the new direction
				return dir;
			}
			else
			{
				// else return current direction
				return forbiddenDir1;
			}
			break;
			
		default:
			break;
	}

	return dir;
}

TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd)
{
	TravelerSegment newSeg;
	switch (currentSeg.dir)
	{
		case Direction::NORTH:
			if (	currentSeg.row < numRows-1 &&
					grid[currentSeg.row+1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row+1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::SOUTH);
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::SOUTH:
			if (	currentSeg.row > 0 &&
					grid[currentSeg.row-1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row-1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::NORTH);
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::WEST:
			if (	currentSeg.col < numCols-1 &&
					grid[currentSeg.row][currentSeg.col+1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col+1;
				newSeg.dir = newDirection(Direction::EAST);
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::EAST:
			if (	currentSeg.col > 0 &&
					grid[currentSeg.row][currentSeg.col-1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col-1;
				newSeg.dir = newDirection(Direction::WEST);
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;
		
		default:
			canAdd = false;
	}
	
	return newSeg;
}

void generateWalls(void)
{
	const unsigned int NUM_WALLS = (numCols+numRows)/4;

	//	I decide that a wall length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_WALL_LENGTH = 3;
	const unsigned int MAX_HORIZ_WALL_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_WALL_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodWall = true;
	
	//	Generate the vertical walls
	for (unsigned int w=0; w< NUM_WALLS; w++)
	{
		goodWall = false;
		
		//	Case of a vertical wall
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_WALLS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*HSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
		// case of a horizontal wall
		else
		{
			goodWall = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_WALLS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*VSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
	}
}


void generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols+numRows)/4;

	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodPart = true;

	for (unsigned int w=0; w< NUM_PARTS; w++)
	{
		goodPart = false;
		
		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_PARTS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*HSP + HSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						grid[row][col] = SquareType::VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
				}
			}
		}
		// case of a horizontal partition
		else
		{
			goodPart = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_PARTS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*VSP + VSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}
			}
		}
	}
}

