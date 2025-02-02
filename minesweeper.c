#include <stdio.h>
#include <locale.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <time.h>
#include <limits.h>

/* constants */

#define MIN_WIDTH 9
#define MAX_WIDTH 39
#define MIN_HEIGHT 9
#define MAX_HEIGHT 20
#define MIN_MINES 1
#define MAX_MINES ((g_width * g_height) / 2)

#define EXPLOSION_COLOR 4

/* globals */
typedef enum _gamemode_t {
    MODE_UNKNOWN = 0,
    MODE_EASY = 1,
    MODE_MEDIUM = 2,
    MODE_HARD = 3,
    MODE_CUSTOM = 4,
} gamemode_t;
int g_mode = MODE_UNKNOWN;

typedef enum {
    STATE_HIDDEN,
    STATE_VISIBLE,
    STATE_FLAG,
    STATE_UNSURE,
} state_t;

char* g_grid;  /* using char* so that way memset cooperates */
char* g_state;
size_t g_gridsize;
int g_selx;
int g_sely;

time_t g_starttime;

/* default values are provided, but later overwritten, so they don't matter */
int g_minecount = 10;
int g_flagged = 0;
int g_width = 9;
int g_height = 9;

int g_wants_color = 1;

/* functions */
void show(int);
void flag(int);
void lose();
void win();
void initGame();
void runGame();
int parseint(char*);
int getrand();
void help(char*);
void cleanup();

/* print to console depending on color or not */
void print_nocolor(short color, char c)
{
    if(color == '!' || color == EXPLOSION_COLOR) attron(A_STANDOUT);
    printw("%c", c);
    if(color == '!' || color == EXPLOSION_COLOR) attroff(A_STANDOUT);
}
void print_color(short color, char c)
{
    attron(COLOR_PAIR(color));
    printw("%c", c);
    attroff(COLOR_PAIR(color));
}

void (*cprint)(short, char) = print_color;

void setup_color()
{
    if(has_colors() == FALSE || !g_wants_color) {
        cprint = print_nocolor;
    } else {
        start_color();
        /* initialize color pairs */
        short fg = COLOR_WHITE, bg = COLOR_BLACK;
        init_pair(' ', fg, bg);
        init_pair('!', COLOR_YELLOW, bg);
        init_pair('1', COLOR_BLUE, bg);
        init_pair('2', COLOR_GREEN, bg);
        init_pair('3', COLOR_RED, bg);
        init_pair('4', COLOR_MAGENTA, bg);
        init_pair('5', COLOR_CYAN, bg);
        init_pair('6', COLOR_RED, bg);
        init_pair('7', COLOR_GREEN, bg);
        init_pair('8', COLOR_MAGENTA, bg);
        init_pair('*', COLOR_RED, bg);
        init_pair(EXPLOSION_COLOR, fg, COLOR_RED);
    }
}

void help(char* name)
{
    printf("\
Usage: %s [--easy | --medium | --hard]\n\
       %s [-w width] [-h height] [-m mines]\n\n", name, name);
    printf("\
Difficulty levels:\n\
  Level:                       Mines    Grid size\n\
    --easy   (--beginner)        10        9x9\n\
    --medium (--intermediate)    40       16x16\n\
    --hard   (--advanced)        99       16x30\n\
You can also specify your own dimensions with the -w, -h, or -m options.\n");
    exit(0);
}

int main(int argc, char** argv)
{
    /* do argument parsing */

    int rc;
    static struct option long_options[] = {
        {"easy", no_argument, &g_mode, MODE_EASY},
        {"beginner", no_argument, &g_mode, MODE_EASY},
        {"medium", no_argument, &g_mode, MODE_MEDIUM},
        {"intermediate", no_argument, &g_mode, MODE_MEDIUM},
        {"hard", no_argument, &g_mode, MODE_HARD},
        {"advanced", no_argument, &g_mode, MODE_HARD},
        {"expert", no_argument, &g_mode, MODE_HARD},
        {"color", no_argument, &g_wants_color, 1},
        {"nocolor", no_argument, &g_wants_color, 0},
        {"help", no_argument, 0, '?'},
        {"width", required_argument, 0, 'w'},
        {"height", required_argument, 0, 'h'},
        {"mines", required_argument, 0, 'm'},
        {0,0,0,0}
    };
    
    setlocale(LC_ALL, "");

    for(int option_index = 0, rc = 0;
            (rc = getopt_long(argc, argv, "w:h:m:?",
                              long_options, &option_index)) != -1;)
    {
        switch(rc) {
            case 0: /* long options */
                break;

            case 'w':
                g_mode = MODE_CUSTOM;
                g_width = parseint(optarg);
                break;

            case 'h':
                g_mode = MODE_CUSTOM;
                g_height = parseint(optarg);
                break;

            case 'm':
                g_mode = MODE_CUSTOM;
                g_minecount = parseint(optarg);
                break;

            case '?':
                help(argv[0]);
                abort();
                break;

            default:
                abort();
        }
    }

    /* setup curses */
    initscr();
    keypad(stdscr, TRUE);
    noecho();

    /* do color alchemy so that it works */
    setup_color();

    /* Say "hello" */
    printw("Welcome to Minesweeper!\n");
    
    /* initialize randomness */
    srand(time(0));

    /* Initialize width and height to match game mode */
    switch(g_mode)
    {
        case MODE_HARD:
            g_width = 30;
            g_height = 16;
            g_minecount = 99;
            break;
        case MODE_MEDIUM:
            g_width = 16;
            g_height = 16;
            g_minecount = 40;
            break;
        case MODE_EASY:
        case MODE_UNKNOWN /* more accurately "unspecified" */:
            g_width = 9;
            g_height = 9;
            g_minecount = 10;
            break;
        default: /* custom or unset gamemode, verify that settings are okay */
            if(g_width > MAX_WIDTH) g_width = MAX_WIDTH;
            else if(g_width < MIN_WIDTH) g_width = MIN_WIDTH;
            
            if(g_height > MAX_HEIGHT) g_height = MAX_HEIGHT;
            else if(g_height < MIN_HEIGHT) g_height = MIN_HEIGHT;

            if(g_minecount > MAX_MINES) g_minecount = MAX_MINES;
            else if(g_minecount < MIN_MINES) g_minecount = MIN_MINES;
            break;
    }
    
    g_starttime = 0;

    /* Allocate grid */
    g_gridsize = g_width * g_height;
    g_grid = malloc(g_gridsize);
    g_state = malloc(g_gridsize);

    if(!g_grid || !g_state) {
        fprintf(stderr, "Memory allocation failed");
        abort();
    }

    initGame();
    runGame();
    cleanup();
}

void initGame()
{
    int i = 0;
        /* move cursor to center */
    g_selx = g_width / 2;
    g_sely = g_height / 2;
    g_flagged = 0;

    /* create grid */
    memset(g_grid, '0', g_gridsize);
    memset(g_state, STATE_HIDDEN, g_gridsize);

    /* fill grid with mines */
    while(i < g_minecount)
    {
        int j = getrand(g_gridsize), x, y;

        /* are we already on a mine? If so, try again. */
        if(g_grid[j] == '*') continue;
        
        g_grid[j] = '*';

        /* now to be responsible we'll increase all surrounding cells
         * This way they store their own mine count, so we don't have to
         * go through and calculate it again. */
        
        x = j % g_width;
        y = j / g_width;

        if(x > 0) {
            if(y > 0 && g_grid[(y-1)*g_width+x-1] != '*') {
                g_grid[(y-1)*g_width+x-1]++;
            }
            if(g_grid[y*g_width+x-1] != '*') {
                g_grid[y*g_width+x-1]++;
            }
            if((y < g_height - 1) && (g_grid[(y+1)*g_width+x-1] != '*')) {
                g_grid[(y+1)*g_width+x-1]++;
            }
        }
        if(x < g_width - 1) {
            if(y > 0 && g_grid[(y-1)*g_width+x+1] != '*') {
                g_grid[(y-1)*g_width+x+1]++;
            }
            if(g_grid[y*g_width+x+1] != '*') {
                g_grid[y*g_width+x+1]++;
            }
            if((y < g_height - 1) && (g_grid[(y+1)*g_width+x+1] != '*')) {
                g_grid[(y+1)*g_width+x+1]++;
            }
        }
        if(y > 0 && g_grid[(y-1)*g_width+x] != '*') {
            g_grid[(y-1)*g_width+x]++;
        }
        if((y < g_height - 1) && (g_grid[(y+1)*g_width+x] != '*')) {
            g_grid[(y+1)*g_width+x]++;
        }

        
        i++;
    }
    
    /* replace '0' with blank space */
    for(i = 0; i < g_gridsize; i++)
    {
        if(g_grid[i] == '0') g_grid[i] = ' ';
    }
    
    g_starttime = 0;
}

int getrand(int max)
{
    /* TODO: more uniform algorithm */
    return rand() % max;
}

void print_cell(int c, int sel)
{
    printw(sel == 1 ? ">" : " ");
   
    if(sel == -2)
        cprint(g_grid[c] == '*' ? EXPLOSION_COLOR : g_grid[c], g_grid[c]);
    else if(sel == -1) {
        if(g_state[c] == STATE_FLAG && g_grid[c] == '*') {
            cprint('!', '*');
        }
        else {
            cprint(g_grid[c], g_grid[c]);
        }
    }
    else {
        if(g_state[c] == STATE_VISIBLE) {
            cprint(g_grid[c], g_grid[c]);
        }
        else if(g_state[c] == STATE_HIDDEN) {
            cprint(' ', '#');
        }
        else if(g_state[c] == STATE_FLAG) {
            cprint('!', '#');
        }
    }
}

void print_grid()
{
    move(2, 0);
    for(int y = 0; y < g_height; y++)
    {
        for(int x = 0; x < g_width; x++)
        {
            print_cell(y * g_width + x, x == g_selx && y == g_sely);
        }
        printw("\n");
    }
    printw("Mines remaining: %2d\n", g_minecount - g_flagged);
    printw("                                           \n");
}

/* like print_grid, but after the game. TODO: dry */
void print_grid_final()
{
    time_t endtime;
    time(&endtime);

    move(2, 0);
    for(int y = 0; y < g_height; y++)
    {
        for(int x = 0; x < g_width; x++)
        {
            print_cell(y * g_width + x, (x == g_selx && y == g_sely) ? -2 : -1);
        }
        printw("\n");
    }
    printw("Time elapsed: %.0f seconds\n", difftime(endtime, g_starttime));
}

void show(int idx)
{
    int chr = g_grid[idx];
    if(g_state[idx] == STATE_HIDDEN)
    {
        g_state[idx] = STATE_VISIBLE;

        if(chr == '*') {
            lose();
            initGame();
        }
        else if(chr == ' ') {
            int x = idx % g_width;
            int y = idx / g_width;
            g_state[idx] == STATE_VISIBLE;
            /* show surrounding areas */
            show(g_width*y + x);
            if(y > 0) show(g_width*(y-1) + x);
            if(y < g_height - 1) show(g_width*(y+1) + x);
            if(x > 0) {
                show(g_width*y + x-1);
                if(y > 0) show(g_width*(y-1) + x-1);
                if(y < g_height - 1) show(g_width*(y+1) + x-1);
            }
            if(x < g_width - 1){
                show(g_width*y + x+1);
                if(y > 0) show(g_width*(y-1) + x+1);
                if(y < g_height - 1) show(g_width*(y+1) + x+1);
            }
        }
    }
}

void flag(int idx)
{
    if(g_state[idx] == STATE_HIDDEN) {
        g_flagged++;
        g_state[idx] = STATE_FLAG;
    } else if(g_state[idx] == STATE_FLAG) {
        g_flagged--;
        g_state[idx] = STATE_HIDDEN;
    }
}

void lose()
{
    mvprintw(0, 0, "You lost ...                          ");
    print_grid_final();
    printw("Press any key to continue, or 'q' to exit. \n");
    if(getch() == 'q') cleanup();
}
void win()
{
    mvprintw(0, 0, "You win!!                             ");
    print_grid_final();
    printw("Press any key to continue, or 'q' to exit. ");
    if(getch() == 'q') cleanup();
}

bool check_win()
{
    /* see if we won or not... */
    for(int i = 0; i < g_gridsize; i++)
    {
        if(g_grid[i] != '*') {
            if(g_state[i] != STATE_VISIBLE) return 0;
        }
    }
    return 1;
}

#define SEL2IDX (g_selx + g_sely*g_width)

void runGame()
{
    int c;
    do {
        if(check_win()) {
            win();
            initGame();
        }
        print_grid();
        if((c = getch()) == 'q') break;
        /* has to be before calls to win() or lose() but after getch() */
        if(g_starttime == 0) time(&g_starttime);
        switch(c)
        {
            case KEY_UP:
                if(g_sely > 0) g_sely--;
                break;
            case KEY_DOWN:
                if(g_sely < g_height - 1) g_sely++;
                break;
            case KEY_LEFT:
                if(g_selx > 0) g_selx--;
                break;
            case KEY_RIGHT:
                if(g_selx < g_width - 1) g_selx++;
                break;
            case 'f':
                flag(SEL2IDX);
                break;
            case 'c':
            case KEY_ENTER:
            case ' ':
                show(SEL2IDX);
                break;
            default: break;
        }
    } while(1);
}

int parseint(char* str)
{
    long o = strtol(str, 0, 10);
    if(o < 1) return 1;
    if(o > CHAR_MAX) return CHAR_MAX;
    return o;
}

void cleanup()
{
    echo();
    keypad(stdscr, FALSE);
    endwin();
    exit(0);
}

