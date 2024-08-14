#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <curses.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#define MIN_Y  2
double DELAY = 0.1;
#define MAX_PLAYERS  2

enum {LEFT=1, UP, RIGHT, DOWN, STOP_GAME=KEY_F(10)};
enum {MAX_TAIL_SIZE=100, START_TAIL_SIZE=3, MAX_FOOD_SIZE=20, FOOD_EXPIRE_SECONDS=10,SEED_NUMBER=3,CONTROLS=2};

// Здесь храним коды управления змейкой
struct control_buttons
{
    int down;
    int up;
    int left;
    int right;
} control_buttons;

struct control_buttons default_controls[CONTROLS] = {{     's',    'w',      'a',       'd'},
                                                     {KEY_DOWN, KEY_UP, KEY_LEFT, KEY_RIGHT}
                                                    };

/*
 Голова змейки содержит в себе
 x,y - координаты текущей позиции
 direction - направление движения
 tsize - размер хвоста
 *tail -  ссылка на хвост
 */
typedef struct snake_t
{
    int x;
    int y;
    int direction;
    size_t tsize;
    struct tail_t *tail;
    struct control_buttons controls;
    int isAI; // Флаг, указывающий, что змейка управляется ИИ
} snake_t;

/*
 Хвост это массив состоящий из координат x,y
 */
typedef struct tail_t
{
    int x;
    int y;
} tail_t;
/*
 Еда — это массив точек, состоящий из координат x,y, времени,
 когда данная точка была установлена, и поля, сигнализирующего,
 была ли данная точка съедена.
 */
struct food
{
    int x;
    int y;
    time_t put_time;
    char point;
    uint8_t enable;
} food[MAX_FOOD_SIZE];

void initFood(struct food f[], size_t size)
{
    struct food init = {0,0,0,0,0};
    for(size_t i=0; i<size; i++)
    {
        f[i] = init;
    }
}
/*
 Обновить/разместить текущее зерно на поле
 */
void putFoodSeed(struct food *fp)
{
    int max_x=0, max_y=0;
    char spoint[2] = {0};
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(fp->y, fp->x, " ");
    fp->x = rand() % (max_x - 1);
    fp->y = rand() % (max_y - 2) + 1; //Не занимаем верхнюю строку
    fp->put_time = time(NULL);
    fp->point = '$';
    fp->enable = 1;
    spoint[0] = fp->point;
    mvprintw(fp->y, fp->x, "%s", spoint);
}

/*
 Разместить еду на поле
 */
void putFood(struct food f[], size_t number_seeds)
{
    for(size_t i=0; i<number_seeds; i++)
    {
        putFoodSeed(&f[i]);
    }
}

void refreshFood(struct food f[], int nfood)
{
    for(size_t i=0; i<nfood; i++)
    {
        if( f[i].put_time )
        {
            if( !f[i].enable || (time(NULL) - f[i].put_time) > FOOD_EXPIRE_SECONDS )
            {
                putFoodSeed(&f[i]);
            }
        }
    }
}
void initTail(struct tail_t t[], size_t size)
{
    struct tail_t init_t= {0,0};
    for(size_t i=0; i<size; i++)
    {
        t[i]=init_t;
    }
}
void initHead(struct snake_t *head, int x, int y)
{
    head->x = x;
    head->y = y;
    head->direction = RIGHT;
}
//========================================================================
void initSnake(snake_t *head[], size_t size, int x, int y,int i, int isAI)
{
    head[i]       = (snake_t*)malloc(sizeof(snake_t));
    tail_t* tail  = (tail_t*) malloc(MAX_TAIL_SIZE*sizeof(tail_t));
    initTail(tail, MAX_TAIL_SIZE);
    initHead(head[i], x, y);
    head[i]->tail     = tail; // прикрепляем к голове хвост
    head[i]->tsize    = size+1;
    head[i]->controls = default_controls[i];
    head[i]->isAI     = isAI;
}

/*
 Движение головы с учетом текущего направления движения
 */
void go(struct snake_t *head)
{
    char ch = '@';
    int max_x=0, max_y=0;
    getmaxyx(stdscr, max_y, max_x); // macro - размер терминала
    mvprintw(head->y, head->x, " "); // очищаем один символ
    switch (head->direction)
    {
        case LEFT:
            if(head->x <= 0)
                head->x = max_x;  
            mvprintw(head->y, --(head->x), "%c", ch);
            break;
        case RIGHT:
            if(head->x >= max_x)
                head->x = 0; 
            mvprintw(head->y, ++(head->x), "%c", ch);
            break;
        case UP:
            if(head->y <= 1)
                head->y = max_y; 
            mvprintw(--(head->y), head->x, "%c", ch);
            break;
        case DOWN:
            if(head->y >= max_y)
                head->y = 0;
            mvprintw(++(head->y), head->x, "%c", ch);
            break;
        default:
            break;
    }
    refresh();
}

void changeDirection(struct snake_t* snake, const int32_t key)
{
    if (key == snake->controls.down && snake->direction != UP)
        snake->direction = DOWN;
    else if (key == snake->controls.up && snake->direction != DOWN)
        snake->direction = UP;
    else if (key == snake->controls.right && snake->direction != LEFT)
        snake->direction = RIGHT;
    else if (key == snake->controls.left && snake->direction != RIGHT)
        snake->direction = LEFT;
}

/*
 Движение хвоста с учетом движения головы
 */
void goTail(struct snake_t *head)
{
    char ch = '*';
    mvprintw(head->tail[head->tsize-1].y, head->tail[head->tsize-1].x, " ");
    for(size_t i = head->tsize-1; i>0; i--)
    {
        head->tail[i] = head->tail[i-1];
        if( head->tail[i].y || head->tail[i].x)
            mvprintw(head->tail[i].y, head->tail[i].x, "%c", ch);
    }
    head->tail[0].x = head->x;
    head->tail[0].y = head->y;
}

//========================================================================
//Проверка того, является ли какое-то из зерен съеденным,
_Bool haveEat(struct snake_t *head, struct food f[])
{
    for (size_t i = 0; i < MAX_FOOD_SIZE; ++i) {
        if ((head->y == f[i].y) && (head->x == f[i].x)) {
            f[i].enable = 0;
            return 1;
        }
    }
    return 0;
}

/*
 Увеличение хвоста на 1 элемент
 */

void addTail(struct snake_t *head)
{
    head->tsize++;
}
//========================================================================
int checkDirection(snake_t* snake, int32_t key)
{
    return (key == snake->controls.down && snake->direction != UP) ||
           (key == snake->controls.up && snake->direction != DOWN) ||
           (key == snake->controls.right && snake->direction != LEFT) ||
           (key == snake->controls.left && snake->direction != RIGHT);
}

// Логика ИИ для выбора направления
void autoChangeDirection(struct snake_t* snake, struct food f[], size_t nfood) {
    int target_x = f[0].x;
    int target_y = f[0].y;
    
    if (snake->x < target_x && snake->direction != LEFT)
        snake->direction = RIGHT;
    else if (snake->x > target_x && snake->direction != RIGHT)
        snake->direction = LEFT;
    else if (snake->y < target_y && snake->direction != UP)
        snake->direction = DOWN;
    else if (snake->y > target_y && snake->direction != DOWN)
        snake->direction = UP;
}

//Вынести тело цикла while из int main() в отдельную функцию update
//и посмотреть, как изменится профилирование
void update(struct snake_t *head, struct food f[], const int32_t key)
{
    clock_t begin = clock();
    
    if (!head->isAI) {
        if (checkDirection(head, key)) {
            changeDirection(head, key);
        }
    } else {
        autoChangeDirection(head, f, SEED_NUMBER); // Управление ИИ
    }

    go(head);
    goTail(head);
    refreshFood(f, SEED_NUMBER);// Обновляем еду
    
    if (haveEat(head,f))
    {
        addTail(head);
    }

    refresh(); // Обновление экрана, вывели кадр анимации

    while ((double)(clock() - begin)/CLOCKS_PER_SEC < DELAY) {
        // Задержка
    }
}
//========================================================================

_Bool isCrush(snake_t * snake)
{
    for (size_t i = 3; i <= snake->tsize - 1; ++i) {
        if ((snake->tail[i].y == snake->y) && (snake->tail[i].x == snake->x)) {
            return 1;
        }
    }
    return 0;
}
//========================================================================

void repairSeed(struct food f[], size_t nfood, struct snake_t *head)
{
    for (size_t i = 0; i < head->tsize; i++) {
        for (size_t j = 0; j < nfood; j++) {
            /* Если хвост совпадает с зерном */
            if ((f[j].enable == 1) && ((f[j].x == head->tail[i].x) && (f[j].y == head->tail[i].y))) {
                putFoodSeed(&f[j]);
            } 
        }
    }

    for (size_t i = 0; i < nfood; i++) {
        for (size_t j = 0; j < nfood; j++) {
            /* Если два зерна на одной точке */
            if ((i != j) && (f[i].x == f[j].x) && (f[i].y == f[j].y)) {
                putFoodSeed(&f[j]);
            }
        }
    }
}

int selectGameMode() {
    int choice = 0;
    while (choice < '1' || choice > '3') {
        clear(); // Очищаем экран
        mvprintw(5, 10, "Select Game Mode:");
        mvprintw(6, 12, "1. Single Player");
        mvprintw(7, 12, "2. Two Players");
        mvprintw(8, 12, "3. Player vs AI");
        refresh();
        choice = getch();
    }
    return choice - '0'; // Преобразуем символ в число
}

int main()
{
    initscr();
    keypad(stdscr, TRUE); // Включаем F1, F2, стрелки и т.д.
    raw();                // Отключаем line buffering
    noecho();            // Отключаем echo() режим при вызове getch
    curs_set(FALSE);    // Отключаем курсор

    // Выбор режима игры
    int gameMode = selectGameMode();

    // Очищаем экран, но оставляем первую строку с инструкциями
    clear();
    mvprintw(0, 1,"Use \"wasd\" for control Player1 and \"arrows\" for control Player2. Press 'F10' for EXIT");
    refresh();

    timeout(0);    // Отключаем таймаут после нажатия клавиши в цикле
    initFood(food, MAX_FOOD_SIZE);
    putFood(food, SEED_NUMBER); // Кладем зерна

    snake_t* snakes[MAX_PLAYERS] = {NULL, NULL};

    if (gameMode == 1) {
        // Однопользовательский режим
        initSnake(snakes, START_TAIL_SIZE, 10, 10, 0, 0);
    } else if (gameMode == 2) {
        // Режим на двух игроков
        initSnake(snakes, START_TAIL_SIZE, 10, 10, 0, 0);
        initSnake(snakes, START_TAIL_SIZE, 30, 10, 1, 0);
    } else if (gameMode == 3) {
        // Режим игрок против ИИ
        initSnake(snakes, START_TAIL_SIZE, 10, 10, 0, 0);
        initSnake(snakes, START_TAIL_SIZE, 30, 10, 1, 1); // Вторая змейка управляется ИИ
    }

    int key_pressed = 0;
    while (key_pressed != STOP_GAME)
    {
        key_pressed = getch(); // Считываем клавишу
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (snakes[i]) {
                update(snakes[i], food, key_pressed);
                if(isCrush(snakes[i])) {
                    mvprintw(1, 1,"Player%d ate himself. Player%d WIN!", i + 1, -(i + 1) + 3);
                    while (key_pressed != STOP_GAME) {
                        key_pressed = getch();
                    }
                    goto end;
                }
                    
                repairSeed(food, SEED_NUMBER, snakes[i]);
            }
        }
    }
end:
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (snakes[i]) {
            free(snakes[i]->tail);
            free(snakes[i]);
        }
    }
    endwin(); // Завершаем режим curses mod
    return 0;
}
