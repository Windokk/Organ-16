// Pong written in Organ 16's C language subset
// Left paddle: bit 0 (up) and bit 8 (down) of port 0, right paddle: same on port 1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

#define N_MAX_SCORE 10

#define PADDLE_HEIGHT 8
#define PADDLE1_X 3
#define PADDLE2_X 125

#define FRAMEBUFFER ((unsigned*)0x8000)
#define FRAMEBUFFER_SIZE 0x4000

#define BLACK 0
#define YELLOW 0xFFE0
#define BLUE 0x551F
#define RED 0xFAEA

#define BALL_COLOR YELLOW
#define PADDLE1_COLOR BLUE
#define PADDLE2_COLOR RED

#define BALL_DELAY 3

#define SCORE_Y 2
#define SCORE1_X 28
#define SCORE2_X 94
#define CELL_SIZE 2
#define DIGIT_COLS 3
#define DIGIT_ROWS 5
#define DIGIT_WIDTH 6
#define DIGIT_HEIGHT 10

unsigned digit_font[10] = {
    0x7b6f, 0x749a, 0x73e7, 0x79e7, 0x49ed,
    0x79cf, 0x7bcf, 0x4927, 0x7bef, 0x79ef
};

int paddle_angles[PADDLE_HEIGHT + 1] = {-2, -2, -1, -1, 0, 1, 1, 2, 2};

int ball_x;
int ball_y;
int ball_dx;
int ball_dy;
int old_ball_x;
int old_ball_y;
int paddle1_y;
int paddle2_y;
int old_paddle1_y;
int old_paddle2_y;
int ball_tick;
int score1;
int score2;

void draw_pixel(int x, int y, unsigned color)
{
    FRAMEBUFFER[y * SCREEN_WIDTH + x] = color;
}

void draw_paddle(int x, int y, unsigned color)
{
    int bottom = y + PADDLE_HEIGHT;
    while (y != bottom) {
        draw_pixel(x, y, color);
        y++;
    }
}

void draw_cell(int x, int y, unsigned color)
{
    draw_pixel(x, y, color);
    draw_pixel(x + 1, y, color);
    draw_pixel(x + 1, y + 1, color);
    draw_pixel(x, y + 1, color);
}

void draw_digit(int x, int y, unsigned color, int digit)
{
    unsigned glyph = digit_font[digit];
    unsigned bit = 1;
    int row;
    int col;
    for (row = 0; row < DIGIT_ROWS; row++) {
        for (col = 0; col < DIGIT_COLS; col++) {
            if (glyph & bit) {
                draw_cell(x + col * CELL_SIZE, y + row * CELL_SIZE, color);
            }
            bit <<= 1;
        }
    }
}

void draw_score1()
{
    draw_digit(SCORE1_X, SCORE_Y, PADDLE1_COLOR, score1);
}

void draw_score2()
{
    draw_digit(SCORE2_X, SCORE_Y, PADDLE2_COLOR, score2);
}

void clear_screen()
{
    unsigned* p = FRAMEBUFFER;
    unsigned* end = FRAMEBUFFER + FRAMEBUFFER_SIZE;
    while (p != end) {
        *p++ = BLACK;
    }
}

void start_round()
{
    clear_screen();
    draw_score1();
    draw_score2();

    ball_x = SCREEN_WIDTH / 2;
    ball_y = SCREEN_HEIGHT / 2;
    paddle1_y = SCREEN_HEIGHT / 2;
    paddle2_y = SCREEN_HEIGHT / 2;

    unsigned r = rand();
    ball_dx = (r & 1) ? 1 : -1;
    ball_dy = (r & 2) ? 1 : -1;
}

void bounce_off_paddle(int paddle_y, int dx)
{
    unsigned hit = ball_y - paddle_y;
    if (hit <= PADDLE_HEIGHT) {
        ball_dy = paddle_angles[hit];
        ball_dx = dx;
    }
}

void move_ball()
{
    if (ball_dx < 0) {
        ball_x--;
    } else if (ball_dx > 0) {
        ball_x++;
    }

    ball_y += ball_dy;
    if (ball_y < 0) {
        ball_y = -ball_y;
        ball_dy = -ball_dy;
    } else if (ball_y > SCREEN_HEIGHT - 1) {
        ball_y = 2 * (SCREEN_HEIGHT - 1) - ball_y;
        ball_dy = -ball_dy;
    }
}

void move_paddles()
{
    unsigned port_a = in0();
    unsigned port_b = in1();

    if (port_a == 1) {
        paddle1_y--;
        old_paddle1_y = paddle1_y + PADDLE_HEIGHT;
    } else if (port_a == 0x100) {
        old_paddle1_y = paddle1_y;
        paddle1_y++;
    }

    if (port_b == 1) {
        paddle2_y--;
        old_paddle2_y = paddle2_y + PADDLE_HEIGHT;
    } else if (port_b == 0x100) {
        old_paddle2_y = paddle2_y;
        paddle2_y++;
    }
}

void draw_frame()
{
    unsigned dy;

    draw_pixel(old_ball_x, old_ball_y, BLACK);
    draw_pixel(PADDLE1_X, old_paddle1_y, BLACK);
    draw_pixel(PADDLE2_X, old_paddle2_y, BLACK);

    draw_pixel(ball_x, ball_y, BALL_COLOR);
    draw_paddle(PADDLE1_X, paddle1_y, PADDLE1_COLOR);
    draw_paddle(PADDLE2_X, paddle2_y, PADDLE2_COLOR);

    if (ball_tick == 0) {
        dy = old_ball_y - SCORE_Y;
        if (dy < DIGIT_HEIGHT) {
            if ((unsigned)(old_ball_x - SCORE1_X) < DIGIT_WIDTH) {
                draw_score1();
            } else if ((unsigned)(old_ball_x - SCORE2_X) < DIGIT_WIDTH) {
                draw_score2();
            }
        }
    }
}

void main()
{
    start_round();

    while (1) {
        ball_tick++;
        if (ball_tick == BALL_DELAY) {
            ball_tick = 0;
            old_ball_x = ball_x;
            old_ball_y = ball_y;

            if (ball_x == 0) {
                score2++;
                if (score2 == N_MAX_SCORE) {
                    return;
                }
                start_round();
                continue;
            }
            if (ball_x == SCREEN_WIDTH - 1) {
                score1++;
                if (score1 == N_MAX_SCORE) {
                    return;
                }
                start_round();
                continue;
            }

            if (ball_x == PADDLE2_X - 1) {
                bounce_off_paddle(paddle2_y, -1);
            } else if (ball_x == PADDLE1_X + 1) {
                bounce_off_paddle(paddle1_y, 1);
            }

            move_ball();
        }

        move_paddles();
        draw_frame();

        old_ball_x = ball_x;
        old_ball_y = ball_y;
    }
}
