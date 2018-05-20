/*
- - - - - - - - - - - - - - - - - - - - - -

Commands to compile and run:

    g++ -o snake snake.cpp -L/usr/X11R6/lib -lX11 -lstdc++
    ./snake

Note: the -L option and -lstdc++ may not be needed on some machines.
*/

/*
 Fonts: https://www.tldp.org/HOWTO/XWindow-User-HOWTO/fontscolors.html
 */

/*
 Color: http://cng.seas.rochester.edu/CNG/docs/x11color.html
 */


#include <iostream>
#include <list>
#include <cstdlib>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <queue>
#include <time.h>
#include <cstring>
#include <string.h>

/*
 * Header files for X functions
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace std;


/*
 * Macros for directions
 */
#define UP 0
#define DOWN 1
#define RIGHT 2
#define LEFT 3

#define GENERAL_GC 0
#define GREEN_GC 1
#define BLUE_GC 2
#define TOMATO_GC 3
#define GRAY_GC 4
#define DIMGRAY_GC 5
#define DARKKHAKI 6

#define NEW_CENT_FT 0
#define TIMES_FT 1
#define UTOPIA_FT 2
#define UTOPIA_S_FT 3

#define START_STG 0x1
#define PLAY_STG 0x2
#define PAUSE_STG 0x4
#define GAMEOVER_STG 0x8

#define NORMAL_FRT 0
#define HEART_FRT 1
#define EVIL_FRT 2

#define MAX_LIVES 5
#define MAX_OBSTACLES 12
#define MIN_OBSTACLES 5

/*
 * Global game state variables
 */
const int Border = 10;
const int BufferSize = 10;
const int width = 800;
const int height = 600;
const int BlockSize = 20;
int FPS = 30;
int speed = 5;

const int RegionStartX = 0;
const int RegionStartY = BlockSize * 2;
const int RegionEndX = width;
const int RegionEndY = height;

int curXspeed = 0;
int curYspeed = 0;

int lastEnterLeaveNotify = 0;
unsigned long lastTimeSpecialFruit = 0;
unsigned long SpecialFruitTimeUsed = 0;

unsigned int score = 0;

unsigned int numOfLives = 1;


/* stage indicates the current stage of the game (start, playing, pause, gameover) */
int curStage = 0;

/*
 * Information to draw on the window.
 */
struct XInfo {
	Display	 *display;
	int		 screen;
	Window	 window;
	GC		 gc[7];
	XFontStruct  *font[4];
	int		width;		// size of window
	int		height;
};


//--------------------------------------------------------------------------------------------------------------------
// Program for usage error handling
//--------------------------------------------------------------------------------------------------------------------
void usage(char *argv[]) {
    cerr << "Usage: " << argv[0] << " frame rate (1 <= frame rate <= 100, default 30)  " << // output the error msg
    "speed (1 <= speed <= 10, default 5)" << endl;
    exit(EXIT_FAILURE); // TERMINATE
} // usage


/*
 * Function to put out a message on error exits.
 */
void error( string str ) {
  cerr << str << endl;
  exit(0);
}

int roundBlockSize(int value) {
	return (value / BlockSize) * BlockSize + (((value % BlockSize) > (BlockSize / 2)) ? 1 : 0)  * BlockSize;
}

int MyMod(int start, int end, int value) {
	if (value >= start) {
		int mod = value % end;
		return (mod < start) ? (mod + start) : mod;
	} else if (value >= 0) {
		return end - start + (abs(value) % end);
	} else { // value < 0, this case is only possible for x coord (start = 0)
		return end - (abs(value) % end);
	}
}

// get microseconds
unsigned long now() {
	timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

void loadFonts(XInfo &xinfo) {
	const char *fontname = "-adobe-new century schoolbook-bold-i-normal--20-140-100-100-p-111-iso8859-10";
    xinfo.font[NEW_CENT_FT] = XLoadQueryFont(xinfo.display, fontname);
    if (!xinfo.font[NEW_CENT_FT]) {
    	cerr << "Cannot load font " << fontname << endl;
        xinfo.font[NEW_CENT_FT] = XLoadQueryFont(xinfo.display, "fixed");
    }

    fontname = "-adobe-times-medium-i-normal--18-180-75-75-p-94-iso8859-2";
    xinfo.font[TIMES_FT] = XLoadQueryFont(xinfo.display, fontname);
    if (!xinfo.font[TIMES_FT]) {
    	cerr << "Cannot load font " << fontname << endl;
        xinfo.font[TIMES_FT] = XLoadQueryFont(xinfo.display, "fixed");
    }

    fontname = "-adobe-utopia-bold-r-normal--33-240-100-100-p-186-iso8859-9";
    xinfo.font[UTOPIA_FT] = XLoadQueryFont(xinfo.display, fontname);
    if (!xinfo.font[UTOPIA_FT]) {
    	cerr << "Cannot load font " << fontname << endl;
        xinfo.font[UTOPIA_FT] = XLoadQueryFont(xinfo.display, "fixed");
    }

    fontname = "-adobe-utopia-regular-r-normal--19-140-100-100-p-105-iso8859-15";
    xinfo.font[UTOPIA_S_FT] = XLoadQueryFont(xinfo.display, fontname);
    if (!xinfo.font[UTOPIA_S_FT]) {
    	cerr << "Cannot load font " << fontname << endl;
        xinfo.font[UTOPIA_S_FT] = XLoadQueryFont(xinfo.display, "fixed");
    }
}


void setFont(XInfo &xinfo, int gc, int font) {
    XSetFont(xinfo.display, xinfo.gc[gc], xinfo.font[font]->fid);
}

/*
 * An abstract class representing displayable things. 
 */
class Displayable {
	public:
		virtual void paint(XInfo &xinfo) = 0;
		int getStage() {
			return stage;
		}
	protected:
		int stage;
};

class Obstacle : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[DARKKHAKI], x, y, xLength, yLength);
	}

	Obstacle() {
		stage = PLAY_STG;

		unsigned int length = rand() % MAX_OBSTACLES;
		length = (length >= MIN_OBSTACLES) ? length : (length + MIN_OBSTACLES);
		length = length * BlockSize;

		baseSide = rand() % 4;

		switch (baseSide) {
			case UP:
				x = MyMod(RegionStartX, RegionEndX, roundBlockSize(rand() % (RegionEndX - BlockSize))); 
				y = RegionStartY;
				xLength = BlockSize;
				yLength = length;
				break;
			case DOWN:
				x = MyMod(RegionStartX, RegionEndX, roundBlockSize(rand() % (RegionEndX - BlockSize))); 
				y = RegionEndY-length;
				xLength = BlockSize;
				yLength = length;
				break;
			case LEFT:
				x = RegionStartX;
				y = MyMod(RegionStartY, RegionEndY, roundBlockSize(rand() % (RegionEndY - BlockSize)));
				xLength = length;
				yLength = BlockSize;			
				break;
			case RIGHT:
				x = RegionEndX-length;
				y = MyMod(RegionStartY, RegionEndY, roundBlockSize(rand() % (RegionEndY - BlockSize)));
				xLength = length;
				yLength = BlockSize;
				break;
		}
	}

	int getX() {
		return x;
	}

	int getY() {
		return y;
	}

	int getXLength() {
		return xLength;
	}

	int getYLength() {
		return yLength;
	}

private:
	unsigned int baseSide; // UP, DOWN, LEFT, RIGHT
	unsigned int x;
	unsigned int y;
	unsigned int xLength;
	unsigned int yLength;
};


class Obstacles : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		for (unsigned i = 0; i < numOfObs; i++) {
			obs[i].paint(xinfo);
		}
	}

	void generateObstacles() {
		/*
		for (unsigned i = 0; i < numOfObs; i++) {
			delete obs[i];
		}
		*/
		delete [] obs;

		numOfObs = rand() % MAX_OBSTACLES;
		numOfObs = (numOfObs >= MIN_OBSTACLES) ? numOfObs : (numOfObs + MIN_OBSTACLES);

		obs = new Obstacle[numOfObs];
		for (unsigned i = 0; i < numOfObs; i++) {
			obs[i] = Obstacle();
		}
	}

	Obstacles() {
		stage = PLAY_STG;
		numOfObs = 0;
		generateObstacles();
	}

	~Obstacles() {
		/*
		for (unsigned i = 0; i < numOfObs; i++) {
			delete obs[i];
		}
		*/
		delete [] obs;
	}

	int getNumOfObs() {
		return numOfObs;
	}

	Obstacle* getObs() {
		return obs;
	}

private:
	Obstacle *obs;
	unsigned int numOfObs;
};

class Fruit : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		if (attribute == NORMAL_FRT) {
			XFillArc(xinfo.display, xinfo.window, xinfo.gc[BLUE_GC], x, y, BlockSize, BlockSize, 0, 360*64);
		} else {
			unsigned long turquoise = 0x40E0D0;
			unsigned long dodgerblue = 0x1E90FF;
			XSetForeground(xinfo.display, xinfo.gc[BLUE_GC], turquoise);
			XDrawArc(xinfo.display, xinfo.window, xinfo.gc[BLUE_GC], x, y, BlockSize-5, BlockSize-5, 0, 360*64);
			XSetForeground(xinfo.display, xinfo.gc[BLUE_GC], dodgerblue);
		}
	}

	Fruit() {
		// generate the x and y value for the fruit
		x = 540;
		y = 300;
		attribute = NORMAL_FRT;

		stage = PLAY_STG;
	}

	int getX() {
		return x;
	}

	int getY() {
		return y;
	}

	int getAttribute() {
		return attribute;
	}

	void generateNewFruit(int &new_x, int &new_y) {
		new_x = roundBlockSize(rand() % (width - BlockSize));
		new_y = roundBlockSize(rand() % (height - BlockSize));
		x = new_x;
		y = new_y;

		int chance = rand() % 12;
		if (chance > 9) { // 2/12 chance evil fruit
			attribute = EVIL_FRT;
		} else if (chance > 6) { // 3/12 chance heart fruit
			attribute = HEART_FRT;
		} else { // 7/12 chance normal fruit
			attribute = NORMAL_FRT;
		}
	}

private:
	int x;
	int y;
	int attribute; // 0 - normal fruit, 1 - heart fruit (increse lives by 1), 2 - evil fruit (decrease by 1)
};


class ScoreDisplay : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		XDrawLine(xinfo.display, xinfo.window, xinfo.gc[GENERAL_GC], 0, RegionStartY-1, width, RegionStartY-1);

		setFont(xinfo, TOMATO_GC, NEW_CENT_FT);
		string scoreStr = "Score : " + to_string(score);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], 20, 29, scoreStr.c_str(), scoreStr.length());


		unsigned i = numOfLives-1;
		for (unsigned j = 0; j < numOfLives && j < MAX_LIVES; j++) {
			int k = j * 35;
			XPoint points[10] = {{250+k, 20}, {255+k, 15}, {260+k, 15}, {263+k, 18}, {263+k, 22},
								 {250+k, 35}, {237+k, 22}, {237+k, 18}, {240+k, 15}, {245+k, 15}};
			XFillPolygon(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], points, 10, Nonconvex, CoordModeOrigin);			
		}

		setFont(xinfo, GRAY_GC, TIMES_FT);

		string text = "Press  p - pause,  r - restart,  q - quit";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[GRAY_GC], 520, 29, text.c_str(), text.length());
	
		string speedStr = "Speed: " + to_string(speed);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[GRAY_GC], 710, 570, speedStr.c_str(), speedStr.length());

		string FPSStr = "FPS: " + to_string(FPS);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[GRAY_GC], 710, 595, FPSStr.c_str(), FPSStr.length());
	}

	ScoreDisplay() {
		x = 20;
		y = 29;
		stage = PLAY_STG;
	}

private:
	int x;
	int y;
};


class PauseDisplay : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		setFont(xinfo, TOMATO_GC, NEW_CENT_FT);
		XPoint points[6] = { {400,200}, {320,240}, {320,320}, {400,430}, {480,320}, {480,240} };
		XFillPolygon(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], points, 6, Convex, CoordModeOrigin);

		string pauseStr = "Resume [y]";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], 350, 270, pauseStr.c_str(), pauseStr.length());

		string restartStr = "Restart [r]";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], 350, 310, restartStr.c_str(), restartStr.length());

		string quitStr = "Quit [q]";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], 365, 350, quitStr.c_str(), quitStr.length());
	}

	PauseDisplay() {
		stage = PAUSE_STG;
	}

};


class StartDisplay : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GENERAL_GC], 0, 0, width, height);

		string name = "Snake";
		setFont(xinfo, DIMGRAY_GC, UTOPIA_FT);
		XSetLineAttributes(xinfo.display, xinfo.gc[GREEN_GC], 4, LineSolid, CapButt, JoinRound);	
		XDrawRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], 339, 135, 115, 38);
		XSetLineAttributes(xinfo.display, xinfo.gc[GREEN_GC], 1, LineSolid, CapButt, JoinRound);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 350, 166, name.c_str(), name.length());

		unsigned long gold = 0xFFD700;
		unsigned long green = 0x008000;
		int x = 530;
		int y = 150;
		XSetForeground(xinfo.display, xinfo.gc[GREEN_GC], gold);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x, y, BlockSize-2, BlockSize-2);
		XSetForeground(xinfo.display, xinfo.gc[GREEN_GC], green);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+1*BlockSize, y, BlockSize-2, BlockSize-2);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+2*BlockSize, y, BlockSize-2, BlockSize-2);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+3*BlockSize, y, BlockSize-2, BlockSize-2);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+4*BlockSize, y, BlockSize-2, BlockSize-2);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+4*BlockSize, y+1*BlockSize, BlockSize-2, BlockSize-2);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+5*BlockSize, y+1*BlockSize, BlockSize-2, BlockSize-2);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], x+6*BlockSize, y+1*BlockSize, BlockSize-2, BlockSize-2);

		string text1 = "Move the snake to eat the fruit, grow the length";
		string text2 = "The snake dies by eating itself or hitting";
		string text3 = "                  the edge or any other obstacles";
		string text4 = "Click the Snake button to start, press [q] to quit";
		string text5 = "By default, the speed is 5 and frame rate is 30";
		setFont(xinfo, DIMGRAY_GC, UTOPIA_S_FT);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 50, 290, text1.c_str(), text1.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 50, 340, text2.c_str(), text2.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 50, 390, text3.c_str(), text3.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 50, 440, text4.c_str(), text4.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 50, 490, text5.c_str(), text5.length());

		string key = "Controls:";
		string key1 = "Up          [w]/[UP]";
		string key2 = "Down    [s]/[DOWN]";
		string key3 = "Left         [a]/[LEFT]";
		string key4 = "Right      [d]/[RIGHT]";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 530, 290, key.c_str(), key.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 560, 340, key1.c_str(), key1.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 560, 390, key2.c_str(), key2.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 560, 440, key3.c_str(), key3.length());
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 560, 490, key4.c_str(), key4.length());

	}

	StartDisplay() {
		stage = START_STG;
	}
};


class GameOverDisplay : public Displayable {
public:
	virtual void paint(XInfo &xinfo) {
		unsigned long almond = 0xFFEBCD;
		unsigned long green = 0x008000;
		XSetForeground(xinfo.display, xinfo.gc[GREEN_GC], almond);
		XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], 0, 0, width, height);
		XSetForeground(xinfo.display, xinfo.gc[GREEN_GC], green);

		setFont(xinfo, DIMGRAY_GC, UTOPIA_FT);
		string gameover = "GAME OVER";
		XSetLineAttributes(xinfo.display, xinfo.gc[DIMGRAY_GC], 10, LineSolid, CapButt, JoinRound);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[DIMGRAY_GC], 300, 260, gameover.c_str(), gameover.length());

		setFont(xinfo, TOMATO_GC, NEW_CENT_FT);
		string scoreStr = "Your score is :  " + to_string(score);
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], 320, 300, scoreStr.c_str(), scoreStr.length());

		setFont(xinfo, GRAY_GC, UTOPIA_S_FT);
		string restartStr = "Restart [r]";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[GRAY_GC], 310, 340, restartStr.c_str(), restartStr.length());

		string quitStr = "Quit [q]";
		XDrawString(xinfo.display, xinfo.window, xinfo.gc[GRAY_GC], 413, 340, quitStr.c_str(), quitStr.length());
	}

	GameOverDisplay() {
		stage = GAMEOVER_STG;
	}
};


Fruit fruit;
Obstacles obstacles;


class Block {
private:
	int x;
	int y;
public:
	Block(int x, int y) : x(x), y(y) { }

	int getX() {
		return x;
	}

	int getY() {
		return y;
	}
};


class Snake : public Displayable {
	public:
		Snake(int x, int y): headX(x), headY(y) {
			stage = PLAY_STG;
			x_speed = BlockSize;
			y_speed = 0;
			lastTimeSpecialFruit = now();

			Block blk1(headX, headY);
			Block blk2(headX-1*BlockSize, headY);
			Block blk3(headX-2*BlockSize, headY);
			Block blk4(headX-3*BlockSize, headY);
			Block blk5(headX-4*BlockSize, headY);
			snakeBody.push_back(blk1);
			snakeBody.push_back(blk2);
			snakeBody.push_back(blk3);
			snakeBody.push_back(blk4);
			snakeBody.push_back(blk5);
		}

		virtual void paint(XInfo &xinfo) {
			/* Draw snake head with different color */
		    for (int i = 0; i < snakeBody.size(); i++) {
		        int blkX = snakeBody[i].getX();
		        int blkY = snakeBody[i].getY();
		        if (i == 0) {
		        	unsigned long gold = 0xFFD700;
		        	unsigned long green = 0x008000;
		        	XSetForeground(xinfo.display, xinfo.gc[GREEN_GC], gold);
		        	XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], blkX, blkY, BlockSize-2, BlockSize-2);
		        	XSetForeground(xinfo.display, xinfo.gc[GREEN_GC], green);
		        } else {
		        	XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[GREEN_GC], blkX, blkY, BlockSize-2, BlockSize-2);
		        }
		    }
		}

		bool onSnakeBody(int x, int y) {
        	for (int i = 1; i < snakeBody.size(); i++) {
        	    if (x == snakeBody[i].getX() && y == snakeBody[i].getY()) {
        	        return true;
        	    }
        	}
        	return false;			
		}

		bool onObstacles(int x, int y) {
			unsigned int numOfObs = obstacles.getNumOfObs();
			Obstacle *obs = obstacles.getObs();

			for (unsigned i = 0; i < numOfObs; i++) {
				if ((x >= obs[i].getX()) && (x < obs[i].getX()+obs[i].getXLength()) &&
					(y >= obs[i].getY()) && (y < obs[i].getY()+obs[i].getYLength())) {
					return true;
				}
			}
			return false;
		}

        bool didHitSnakeitself() {
        	if (onSnakeBody(headX, headY)) {
        		cout << "Hit snake itself!" << endl;
        		return true;
        	}
        	return false;
        }

        void regenerateFruit(Fruit &frt) {
			/* regenerate another fruit */
			int new_x, new_y;
			for (;;) {
				frt.generateNewFruit(new_x, new_y); // new fruit position
				// make sure new fruit is not overlapping with the snake body, obstacles, and fruit is in proper region
				if ((!onSnakeBody(new_x, new_y)) && (headX != new_x) && (headY != new_y) &&
					(!onObstacles(new_x, new_y)) && (new_y >= RegionStartY)) {
					cout << "X: " << new_x << " Y: " << new_y << endl;
					if (frt.getAttribute() == HEART_FRT || frt.getAttribute() == EVIL_FRT) {
						lastTimeSpecialFruit = now();
					}
					break;
				}
			}
        }

		bool didEatFruit(Fruit &frt, int &attribute) {
			if (frt.getX() == headX && frt.getY() == headY) {
				if (frt.getAttribute() == NORMAL_FRT) {
					score++;
					cout << "Eat Scoring Fruit!" << endl;
					attribute = NORMAL_FRT;
				} else if (frt.getAttribute() == HEART_FRT) {
					if (numOfLives < 5) {
						numOfLives++;
						cout << "Eat Heart Fruit, incresed lives by 1" << endl;
					} else {
						cout << "Hit Max Lives" << endl;
					}
					attribute = HEART_FRT;
				} else { // evil fruit
					numOfLives--;
					cout << "Eat Evil Fruit, decreased lives by 1" << endl;
					attribute = EVIL_FRT;
					if (numOfLives == 0) {
						curStage = GAMEOVER_STG;
						return true;
					}
				}

				regenerateFruit(frt);

				return true;
			} else {
				if ((frt.getAttribute() == HEART_FRT || frt.getAttribute() == EVIL_FRT) &&
					((now() - lastTimeSpecialFruit) > 35000000/speed)) { // regenerate after 5s if special fruits
					regenerateFruit(frt);
				}
				return false;
			}
        }
/*
        bool didHittheWall() {
        	if (headX < RegionStartX || headX > RegionEndX || headY < RegionStartY || headY > RegionStartY) {
        		cout << "Hit the wall!" << endl;
        		return true;
        	}
        	return false;
        }
*/
		bool didHitObstacle() {
			if (onObstacles(headX, headY)) {
				cout << "Hit the obstables" << endl;
				return true;
			}
			return false;
        }

        void didDead() {
		    if (didHitSnakeitself() || didHitObstacle()) {
		    	numOfLives--;
		    	if (numOfLives == 0) {
		    		curStage = GAMEOVER_STG;
		    		cout << "Game Over!" << endl;
		    	} else {
		    		//TODO
		    	}
		    }
        }

		void move(XInfo &xinfo) {
			if (curStage != PLAY_STG) return;

			headX = MyMod(RegionStartX, RegionEndX, headX + x_speed);
			headY = MyMod(RegionStartY, RegionEndY, headY + y_speed);

			didDead();

			int attribute;
			if (!(didEatFruit(fruit, attribute) && attribute == NORMAL_FRT)) {
				snakeBody.pop_back();
			}
			Block blk(headX, headY);
			snakeBody.push_front(blk);

		}

		void changeDirection(int direction) {
			if (curStage != PLAY_STG) return;

			// snake starts with length of 5, so safe to access first 2 block for direction verification
			int blk1X = snakeBody[0].getX();
			int blk2X = snakeBody[1].getX();

			bool movingAlongYcoord = (blk1X == blk2X); // if x coords are the same, so moving along y direction

			switch (direction) { // only can change to directions that are perpendicular to the original direction
				case UP:
					if (!movingAlongYcoord) {
						y_speed = -abs(x_speed);
						x_speed = 0;
					}
					break;
				case DOWN:
					if (!movingAlongYcoord) {
						y_speed = abs(x_speed);
						x_speed = 0;
					}
					break;
				case RIGHT:
					if (movingAlongYcoord) {
						x_speed = abs(y_speed);
						y_speed = 0;
					}
					break;
				case LEFT:
					if (movingAlongYcoord) {
						x_speed = -abs(y_speed);
						y_speed = 0;
					}
					break;
			}
		}

		int getXspeed() {
			return x_speed;
		}

		int getYspeed() {
			return y_speed;
		}

		void setXspeed(int value) {
			x_speed = value;
		}

		void setYspeed(int value) {
			y_speed = value;
		}

	private:
		int headX;
		int headY;
		int x_speed;
		int y_speed;
        deque<Block> snakeBody;

};


list<Displayable *> dList;           // list of Displayables
Snake snake(340, 300);
ScoreDisplay scoreDisplay;
StartDisplay startDisplay;
PauseDisplay pauseDisplay;
GameOverDisplay gameoverDisplay;


/*
 * Initialize X and create a window
 */
void initX(int argc, char *argv[], XInfo &xInfo) {
	XSizeHints hints;
	unsigned long white, black;

   /*
	* Display opening uses the DISPLAY	environment variable.
	* It can go wrong if DISPLAY isn't set, or you don't have permission.
	*/	
	xInfo.display = XOpenDisplay( "" );
	if ( !xInfo.display )	{
		error( "Can't open display." );
	}
	
   /*
	* Find out some things about the display you're using.
	*/
	xInfo.screen = DefaultScreen( xInfo.display );

	white = XWhitePixel( xInfo.display, xInfo.screen );
	black = XBlackPixel( xInfo.display, xInfo.screen );

	hints.x = 100;
	hints.y = 100;
	hints.width = 800;
	hints.height = 600;
	hints.flags = PPosition | PSize;

	xInfo.window = XCreateSimpleWindow( 
		xInfo.display,				// display where window appears
		DefaultRootWindow( xInfo.display ), // window's parent in window tree
		hints.x, hints.y,			// upper left corner location
		hints.width, hints.height,	// size of the window
		Border,						// width of window's border
		white,						// window border colour
		black );					// window background colour
		
	XSetStandardProperties(
		xInfo.display,		// display containing the window
		xInfo.window,		// window whose properties are set
		"Snake",		// window's title
		"Snake",			// icon's title
		None,				// pixmap for the icon
		argv, argc,			// applications command line args
		&hints );			// size hints for the window

	/* 
	 * Create Graphics Contexts
	 */
	int i = 0;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], WhitePixel(xInfo.display, xInfo.screen));
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillSolid);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);


	i = 1;
	unsigned long green = 0x008000;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], green);
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillSolid);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);

	i = 2;
	unsigned long dodgerblue = 0x1E90FF;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], dodgerblue);
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillSolid);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     5, LineSolid, CapButt, JoinRound);

	i = 3;
	unsigned long tomato = 0xFF6347;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], tomato);
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillSolid);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);

	i = 4;
	unsigned long gray = 0x808080;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], gray);
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillSolid);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);

	i = 5;
	unsigned long dimgray = 0x696969;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], dimgray);
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillOpaqueStippled);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);	


	i = 6;
	unsigned long darkkhaki = 0xBDB76B;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], darkkhaki);
	XSetBackground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillOpaqueStippled);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);


	loadFonts(xInfo);

	XSelectInput(xInfo.display, xInfo.window, 
		ButtonPressMask | KeyPressMask | 
		PointerMotionMask | 
		EnterWindowMask | LeaveWindowMask |
		StructureNotifyMask);  // for resize events

	/*
	 * Put the window on the screen.
	 */
	XMapRaised( xInfo.display, xInfo.window );
	XFlush(xInfo.display);
}

/*
 * Function to repaint a display list
 */
void repaint( XInfo &xinfo) {
	list<Displayable *>::const_iterator begin = dList.begin();
	list<Displayable *>::const_iterator end = dList.end();

	XClearWindow( xinfo.display, xinfo.window );
	
	// get height and width of window (might have changed since last repaint)
/*
	XWindowAttributes windowInfo;
	XGetWindowAttributes(xinfo.display, xinfo.window, &windowInfo);
	unsigned int height = windowInfo.height;
	unsigned int width = windowInfo.width;
*/
	// big black rectangle to clear background
    
	// draw display list
	while( begin != end ) {
		Displayable *d = *begin;
		if (curStage & d->getStage()) {
			d->paint(xinfo);
		}
		begin++;
	}
	XFlush( xinfo.display );
}


void pause(Snake &snk) {
	if (curStage != PLAY_STG) return; // can only pause during PLAY stage
	SpecialFruitTimeUsed = now() - lastTimeSpecialFruit;
	curStage = PLAY_STG | PAUSE_STG;
	curXspeed = snk.getXspeed();
	curYspeed = snk.getYspeed();
	snk.setXspeed(0);
	snk.setYspeed(0);
}


void resume(Snake &snk) {
	if (curStage != (PLAY_STG | PAUSE_STG)) return; // can only resume during PAUSE stage
	curStage = PLAY_STG;
	snk.setXspeed(curXspeed);
	snk.setYspeed(curYspeed);
	lastTimeSpecialFruit = now() - SpecialFruitTimeUsed;
}


void handleKeyPress(XInfo &xinfo, XEvent &event) {
	KeySym key;
	char text[BufferSize];
	
	/*
	 * Exit when 'q' is typed.
	 * This is a simplified approach that does NOT use localization.
	 */
	int i = XLookupString( 
		(XKeyEvent *)&event, 	// the keyboard event
		text, 					// buffer when text will be written
		BufferSize, 			// size of the text buffer
		&key, 					// workstation-independent key symbol
		NULL );					// pointer to a composeStatus structure (unused)
	if (i == 1) {
		printf("Got key press -- %c\n", text[0]);

		switch (text[0]) { // handle upper case and lower case WASD/wasd keys
			case 'q':
			case 'Q':
				error("Terminating normally."); // can quit at any stage
			case 'r':
			case 'R':
				if (curStage == START_STG) break; // cannot restart at the start stage
				curStage = PLAY_STG;
				numOfLives = 1;
				snake = Snake(340, 300);
				fruit = Fruit();
				obstacles.generateObstacles();
				score = 0;
				break;
			case 'p':
			case 'P':
				pause(snake);
				break;
			case 'y':
			case 'Y':
				resume(snake);
				break;
			case 'w':
			case 'W':
				snake.changeDirection(UP);
				break;
			case 'a':
			case 'A':
				snake.changeDirection(LEFT);
				break;
			case 's':
			case 'S':
				snake.changeDirection(DOWN);
				break;
			case 'd':
			case 'D':
				snake.changeDirection(RIGHT);
				break;
		}
	}

	if (key) {
		switch (key) {
			case XK_Left:
				snake.changeDirection(LEFT);
				break;
			case XK_Right:
				snake.changeDirection(RIGHT);
				break;
			case XK_Up:
				snake.changeDirection(UP);
				break;
			case XK_Down:
				snake.changeDirection(DOWN);
				break;
		}
	}
}

void handleButtonPress(XInfo &xinfo, XEvent &event) {
	int x = event.xbutton.x;
	int y = event.xbutton.y;

	/* The green box starts at (339, 135), and has a width 115 and height 38 */
	if ((x >= 339) && (x <= (339+115)) && (y >= 135) && (y <= (135+38))) {
		curStage = PLAY_STG;
	}

	//XDrawRectangle(xinfo.display, xinfo.window, xinfo.gc[TOMATO_GC], 405, 235, 22, 24);
	/* Back Door to REBORN when dead: click the 'O' in GAME OVER around pixel (405,235) width 22, height 24 */
	if ((curStage == GAMEOVER_STG) && (x >= 405) && (x <= (405+22)) && (y >= 235) && (y <= (235+24))) {
		numOfLives++;
		curStage = PLAY_STG;
	}

}

void handleAnimation(XInfo &xinfo, int inside) {
	if (inside == 1 && lastEnterLeaveNotify == 0) { // EnterNotify
		resume(snake);
	} else if (inside == 0 && lastEnterLeaveNotify == 1) { // LeaveNotify
		pause(snake);
	}

	if (inside != lastEnterLeaveNotify) {
		lastEnterLeaveNotify = inside;
	}
}

void eventLoop(XInfo &xinfo) {
	// Add stuff to paint to the display list
	dList.push_front(&pauseDisplay);
	dList.push_front(&snake);
    dList.push_front(&fruit);
    dList.push_front(&obstacles);
    dList.push_front(&scoreDisplay);
    dList.push_front(&startDisplay);
	dList.push_front(&gameoverDisplay);

	XEvent event;
	unsigned long lastRepaint = 0;
	int inside = 0;

	unsigned long lastMove = 0;

	curStage = START_STG;

	while( true ) {
		if (XPending(xinfo.display) > 0) {
			XNextEvent( xinfo.display, &event );
			cout << "event.type=" << event.type << "\n";
			switch( event.type ) {
				case KeyPress:
					handleKeyPress(xinfo, event);
					break;
				case EnterNotify:
					inside = 1;
					break;
				case LeaveNotify:
					inside = 0;
					break;
				case ButtonPress:
					handleButtonPress(xinfo, event);
					break;
			}
		}

		unsigned long repaintEnd = now(); // time in microseconds
		if (repaintEnd - lastRepaint > 1000000/FPS) {
			handleAnimation(xinfo, inside);
			repaint(xinfo);
			lastRepaint = now();
		}

		unsigned long moveEnd = now(); // time in microseconds
		if (moveEnd - lastMove > 500000/speed) {
			snake.move(xinfo);
			lastMove = now();
		}

		if (XPending(xinfo.display) == 0) {
			usleep(1000000/FPS - (repaintEnd - lastRepaint));
		}
	}
}


/*
 * Start executing here.
 *	 First initialize window.
 *	 Next loop responding to events.
 *	 Exit forcing window manager to clean up - cheesy, but easy.
 */
int main ( int argc, char *argv[] ) {
	enum {	MAX_FPS = 100,
			MIN_FPS = 1,
			MAX_SPEED = 10,
			MIN_SPEED = 1 };

	/* Handle command line argument */
    switch (argc) {
    	case 3:
    	    speed = atoi(argv[2]);
        	if (speed < MIN_SPEED || speed > MAX_SPEED) {
        		usage(argv);
        	}
        	// FALL THROUGH
      	case 2:
      		FPS = atoi(argv[1]);
        	if (FPS < MIN_FPS || FPS > MAX_FPS) {
          		usage(argv);
        	}
        	// FALL THROUGH
      	case 1: // all defaults
        	break;
      	default: // wrong number of options
        	usage(argv);
    } // switch

    srand((unsigned int)time(0)); // random number seed
	XInfo xInfo;

	initX(argc, argv, xInfo);
	eventLoop(xInfo);
	XCloseDisplay(xInfo.display);
}
