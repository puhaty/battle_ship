#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CLEAR_INPUT_BUFFER {char ch; while((ch=getchar()) != '\n' && ch != EOF);}

typedef enum bool {false=0, true=1} bool;
typedef enum contents {EMPTY=0, FRIGATE, WARSHIP, DESTROYER, SUBMARINE, BOAT} contents;
typedef enum status {UNKNOWN=0, KNOWN} status;
typedef enum direction {EAST=0, NORTH, WEST, SOUTH} direction;
typedef enum state {PRISTINE=0, HIT, SUNK} state;
typedef enum orientation {UNDETERMINED=0, VERTICAL, HORIZONTAL} orientation;

typedef struct ship {		//ship variables 
	contents name;			/*type of ship*/
	int location[2];		/*space to store the ship's anchor point*/
	direction direction;	/*which way is the ship going?*/
	int health;				/*number of remaining hits - at 0 the ship sinks*/
} ship;

typedef struct player {		//player variables
	ship ships[5];			/*space to hold player's ships*/
	bool floating[5];
} player;

typedef struct space {
	contents contents;
	status status;
} space;


typedef struct storage			//computer variables
{
	int skill;					/*Computer skill level*/
	state state[5];				/*state of player ships*/
	int endpoints[5][4];		/*locations of player ships (once found)*/
	orientation orientation[5];
	bool patternLikeParity;		/*which diagonals do we select?*/
} storage;


space playerBoard[10][10];
space opponentBoard[10][10];
player local;
player * opponent;			/*dummy pointer to either Computer or remote*/
bool isLocalTurn;
player Computer;
storage memory;		


void initBoards(void)		/*Initialize player and computer grids*/
{
	int i, j;
	space sp;
	
	sp.status=UNKNOWN;		/*initialize space data*/
	sp.contents=EMPTY;
	for (i=0; i<10; i++)
		for (j=0; j<10; j++)	
		{
			playerBoard[i][j] = sp;
			opponentBoard[i][j] = sp;	/*copy empty space into each board*/
		}
}

void initPlayer(void)		/*initialize player data*/
{
	int i;
	
	for (i=0; i<5; i++)		/*initialize ships*/
	{
		local.floating[i] = true;
		local.ships[i].name = (contents)(i+1);
		local.ships[i].health =  5-i;
		if (i>=3)
			local.ships[i].health++;
	}
}

int placeShip(ship * sh, int x, int y, direction dir)	/*place ship on the board*/
{
	int i, j, test_x=x, test_y=y;
	space (*board)[10];
	
	if (isLocalTurn)
		board=playerBoard;
	else
		board=opponentBoard;
	
	/*First, validate the proposed ship placement*/
	for (i=0; i<sh->health; i++)
	{
		if (test_x<0 || test_x>=10 || test_y<0 || test_y>=10)	/*ship is off-map*/
			return 1;
		
		if (board[test_x][test_y].contents!=EMPTY)	/*ship overlaps another*/
			return 1;
		
		switch (dir) {
			case NORTH:
				test_y--;
				break;
			case SOUTH:
				test_y++;
				break;
			case EAST:
				test_x++;
				break;
			case WEST:
				test_x--;
				break;
		}
	}
	
	sh->location[0]=x;
	sh->location[1]=y;
	sh->direction=dir;
	
	for (j=0; j<sh->health; j++)	//set the board to show occupancy
	{
		board[x][y].contents=sh->name;
		switch (dir) {
			case NORTH:
				y--;
				break;
			case SOUTH:
				y++;
				break;
			case EAST:
				x++;
				break;
			case WEST:
				x--;
				break;
		}
	}
	
	return 0;				
}

int checkHit(int x, int y)		//Is it a hit?
{
	space (*board)[10];
	player * pl;
	bool hit, sunk;
	
	if (isLocalTurn)
	{
		board=opponentBoard;
		pl=opponent;
	} else
	{
		board=playerBoard;
		pl=&local;
	}
	
	if (board[x][y].contents!=EMPTY)
	{							/*it's a hit!*/
		pl->ships[board[x][y].contents-1].health--;
		hit = true;
		if (pl->ships[board[x][y].contents-1].health==0)
		{
			sunk = true;
			pl->floating[board[x][y].contents-1]=false;
		} else
			sunk = false;
	} else						/*it's a miss*/
		hit = sunk = false;
	
	board[x][y].status=KNOWN;	/*now we know with certainty what's there*/
	return (int)hit+(int)sunk;
}

int RNG(int low, int high)		/*generate a pseudo-random number*/
{
	int z=low + (int) (0.5 + (high-low+1) * (double)rand() / ((double)(RAND_MAX) + 1.0));
	/*adding 0.5 then casting as int implements an approximation of rounding*/
	if (z>high) /*this statement adds uniformity that was destroyed*/
		z=low;	/*by the rounding implementation*/
	return z;
}

void place_Computer_Ships(void)			/*randomly place Computer ships on the board*/
{
	int i, x, y;
	direction dir;
	bool err;
	
	for (i=0; i<5; i++)
	{
		do {
			x=RNG(0, 9);		/*select an endpoint...*/
			y=RNG(0, 9);
			dir=RNG(0,3);	/*...and a direction*/
			err=placeShip(&Computer.ships[i], x, y, dir);	/*then place the ship*/
		} while (err==true);	/*on the board in the chosen location*/
	}

	return;
}

void init_Computer(int level)			/*initialize Computer player*/
{
	int i;
	
	for (i=0; i<5; i++)			/*initialize ships and game state*/
	{
		Computer.floating[i] = true;
		Computer.ships[i].name = (contents)(i+1);
		Computer.ships[i].health =  5-i;
		if (i>=3)
			Computer.ships[i].health++;
		memory.state[i]=PRISTINE;
		memory.orientation[i]=UNDETERMINED;
	}
	
	place_Computer_Ships();
	
	memory.skill=level;
}



void Computer_Fire(int * x, int * y)	/*select a point to shoot*/
{
	int i, j;
	direction dir;
	bool targeted=false;
	
	do {
			for (i=4; i>=0; i--)	/*prioritize smaller ships*/
				if (memory.state[i]==HIT)
				{
					j=2*RNG(0,1);	/*select an endpoint on target ship*/
					if (memory.orientation[i]!=UNDETERMINED)
					{
						dir=(2*RNG(0,1)+memory.orientation[i])%4;
							/*choose a direction fitting to the known orientation*/
					} else {
						dir=RNG(0,3);	/*choose one of the 4 adjacent points at random*/
					}
					switch (dir) {	/*use the chosen point and direction to fire!*/
						case NORTH:
							*x=memory.endpoints[i][j];
							*y=memory.endpoints[i][1+j]-1;
							break;
						case SOUTH:
							*x=memory.endpoints[i][j];
							*y=memory.endpoints[i][1+j]+1;
							break;
						case WEST:
							*x=memory.endpoints[i][j]-1;
							*y=memory.endpoints[i][1+j];
							break;
						case EAST:
							*x=memory.endpoints[i][j]+1;
							*y=memory.endpoints[i][1+j];
							break;
					}
					targeted=true;
					break;
				}
		if (targeted==false)	/*no previously hit ships to pick on*/
		{
			*x=RNG(0,9);
			*y=RNG(0,9);
		}
	} while (playerBoard[*x][*y].status!=UNKNOWN || *y>9 || *y<0 || *x>9 || *x<0);
	/*shoot only at legal points which have not previously been fired at*/
}

void Computer_Store(int x, int y, int shipState)
{
	int hitShip, distance;
	
	hitShip=playerBoard[x][y].contents-1;	/*get identity of hit ship*/
	if (memory.state[hitShip]==PRISTINE)	/*it's the first hit on the ship*/
	{
		memory.endpoints[hitShip][0]=x;	/*save the location as both endpoints*/ 
		memory.endpoints[hitShip][1]=y;
		memory.endpoints[hitShip][2]=x;
		memory.endpoints[hitShip][3]=y;
	} else 	if (shipState==HIT)	/*the ship wasn't sunk*/
	{							/*save the new location as an endpoint*/
		distance=memory.endpoints[hitShip][0]-x+memory.endpoints[hitShip][1]-y;
		if (distance==1 || distance==-1)
		{
			memory.endpoints[hitShip][0]=x;
			memory.endpoints[hitShip][1]=y;
		} else
		{
			memory.endpoints[hitShip][2]=x;
			memory.endpoints[hitShip][3]=y;
		}
	}
	if (memory.skill>2 && memory.orientation[hitShip]==UNDETERMINED && memory.state[hitShip]==HIT)
	/*skill is high enough, direction was unknown, and ship is previously hit*/
	{
		if (memory.endpoints[hitShip][0]==memory.endpoints[hitShip][2])
			memory.orientation[hitShip]=VERTICAL;
		else
			memory.orientation[hitShip]=HORIZONTAL;
	}
	memory.state[hitShip]=shipState;		/*store the ship as hit/sunk*/

	return;
}






void initOpponent(void)
{
	int level;
    level = 3;
    isLocalTurn=false;
	init_Computer(level);
	opponent=&Computer;
	printf("Your opponent is ready\n");
	return;
}

char backTranslate(int row)			/*turn int-format rows into letter format*/
{
	return (row + 'A'); 			/*0-9 --> Uppercase A-J*/
}

void drawBoard(bool showHidden)
{
	int i,j,k;
	printf("\n");
	if (showHidden)
		printf("       Opponent's Board           Your Board\n");
	else
		printf("       Your Grid                  Your Board\n");
	printf("   1 2 3 4 5 6 7 8 9 10      1 2 3 4 5 6 7 8 9 10\n");
	for (i=0; i<10; i++)
	{
		printf(" %c ", backTranslate(i));
		for (j=0; j<10; j++)	/*loop over all spaces*/
		{
			if (opponentBoard[j][i].status==KNOWN)
			{
				switch (opponentBoard[j][i].contents) {
					case EMPTY:
						printf("X ");
						break;
					case FRIGATE:
						printf("# ");
						break;
					case WARSHIP:
						printf("# ");
						break;
					case DESTROYER:
						printf("# ");
						break;
					case SUBMARINE:
						printf("# ");
						break;
					case BOAT:
						printf("# ");
						break;
				}
			} else 
			{
				if (showHidden)
				{
					switch (opponentBoard[j][i].contents) {
						case EMPTY:
							printf("- ");
							break;
						case FRIGATE:
							printf("F ");
							break;
						case WARSHIP:
							printf("W ");
							break;
						case DESTROYER:
							printf("D ");
							break;
						case SUBMARINE:
							printf("S ");
							break;
						case BOAT:
							printf("B ");
							break;
					}
				} else
					printf("- ");
			}
		}
		printf("    %c ", backTranslate(i));
		for (k=0; k<10; k++)
		{
			if (playerBoard[k][i].status==KNOWN)
			{
				switch (playerBoard[k][i].contents) {
					case EMPTY:
						printf("X ");
						break;
					case FRIGATE:
						printf("# ");
						break;
					case WARSHIP:
						printf("# ");
						break;
					case DESTROYER:
						printf("# ");
						break;
					case SUBMARINE:
						printf("# ");
						break;
					case BOAT:
						printf("# ");
						break;
				}
			} else
			{
				switch (playerBoard[k][i].contents) {
					case EMPTY:
						printf("- ");
						break;
					case FRIGATE:
						printf("F ");
						break;
					case WARSHIP:
						printf("W ");
						break;
					case DESTROYER:
						printf("D ");
						break;
					case SUBMARINE:
						printf("S ");
						break;
					case BOAT:
						printf("B ");
						break;
				}
			}
		}
		printf("\n");
	}
	if (showHidden==false)
	{
		printf("\nF = FRIGATE    W = WARSHIP  D = DESTROYER\n");
		printf("S = SUBMARINE  B =  BOAT     X = Miss   # = Hit\n");
	}
}

int translate(char row)				/*turn user input char into an int*/
{
	if (row <= 'J' && row >= 'A')
		return (row - 'A');		/*Uppercase A-J --> 0-9*/
	else if (row <= 'j' && row >= 'a')
		return (row - 'a');		/*Lowercase a-j --> 0-9*/
	else
		return -1;		/*bogus value that will result in asking for 'row' again*/
}

void getShipPlacement(int * x, int * y, direction * dir)
{
	char y_char, dir_char;
	bool err;
	
	do {
		err=false;
		printf("Row [A-J]:");
		scanf("%c", &y_char);
        CLEAR_INPUT_BUFFER;
		*y=translate(y_char);
		if (*y < 0 || *y > 9)
		{
			err=true;
			printf("Bad row value\n");
		}
	} while (err);
	do {
		err=false;
		printf("Column [1-10]:");
		scanf("%i", x);
        CLEAR_INPUT_BUFFER;
		(*x)--;
		if (*x < 0 || *x > 9)
		{
			err=true;
			printf("Bad column value\n");
		}
	} while (err);
	do {
		err=false;
		printf("Direction [NSEW]:");
		scanf("%c", &dir_char);
		CLEAR_INPUT_BUFFER;
		switch (dir_char) {
			case 'N':
			case 'n':
				*dir=NORTH;
				break;
			case 'S':
			case 's':
				*dir=SOUTH;
				break;
			case 'E':
			case 'e':
				*dir=EAST;
				break;
			case 'W':
			case 'w':
				*dir=WEST;
				break;
			default:
				err=true;
				printf("Bad direction value\n");
				break;
		}
	} while (err);
	return;
}


void placeLocalShips(void)
{
	int x, y;
	direction dir;
	bool err;
	
	printf("for each ship, input a location (row, column) and a direction\n (North, South, East, or West\n");
	isLocalTurn=true;
	do {
		drawBoard(false);
		printf("Please place your FRIGATE\n");
		getShipPlacement(&x, &y, &dir);
		err=placeShip(&local.ships[FRIGATE-1], x, y, dir);
	} while (err);
	do {
		drawBoard(false);
		printf("Please place your WARSHIP\n");
		getShipPlacement(&x, &y, &dir);
		err=placeShip(&local.ships[WARSHIP-1], x, y, dir);
	} while (err);
	do {
		drawBoard(false);
		printf("Please place your DESTROYER\n");
		getShipPlacement(&x, &y, &dir);
		err=placeShip(&local.ships[DESTROYER-1], x, y, dir);
	} while (err);
	do {
		drawBoard(false);
		printf("Please place your SUBMARINE\n");
		getShipPlacement(&x, &y, &dir);
		err=placeShip(&local.ships[SUBMARINE-1], x, y, dir);
	} while (err);
	do {
		drawBoard(false);
		printf("Please place your BOAT\n");
		getShipPlacement(&x, &y, &dir);
		err=placeShip(&local.ships[BOAT-1], x, y, dir);
	} while (err);
	return;
}


void opponentFire(int * x, int * y)
{
	Computer_Fire(x, y);
	printf("Your opponent fires at %c%i...\n", backTranslate(*y), 1+*x);
	return;
}


void takeTurn(void)		/*move through a single turn*/
{
	int x,y;
	char a;
	int result;
	
	drawBoard(false);

	if (isLocalTurn)
	{
		do {
			printf("Where would you  like to fire [Row][Column]?\n");
			scanf("%c%d", &a, &x);
			CLEAR_INPUT_BUFFER;
			x--;
			y=translate(a);
		} while (x<0 || x>9 || y<0 ||y>9);
	} else
	{
		opponentFire(&x, &y);
	}
	result=checkHit(x, y);
	if (result>0)
	{
		printf("Hit!\n");
		if (opponent==&Computer && isLocalTurn==false)
			Computer_Store(x, y, result);
	}
    else
    printf("miss!!\n");
	return;
}








int main(void)
{
	int i;
	bool localAlive, opponentAlive;
	
	srand(time(NULL));
	
	printf("Welcome to Warboats!\n\n");
	

	initBoards();
	initPlayer();
	placeLocalShips();
	initOpponent();
	
	/*randomly choose a player to start*/
	isLocalTurn=RNG(0,1);
	printf("You will go ");
	if (isLocalTurn)
		printf("first\n");
	else
		printf("second\n");
	do {				/*loops while both players have at least one ship*/
		localAlive=opponentAlive=false;
		takeTurn();
		isLocalTurn=(isLocalTurn+1)%2;
		for (i=0; i<5; i++)
		{
			if (local.floating[i])
				localAlive=true;
			if (opponent->floating[i])
				opponentAlive=true;
		}
	} while (localAlive && opponentAlive);
	
	drawBoard(true);	/*draw end-of-game board*/
	
	if (localAlive)
		printf("You won!\n");
	else
		printf("You lost.\n");
	return 0;
}
