#ifndef MOUSE_H_INCLUDED
#define MOUSE_H_INCLUDED 1

enum {
    MOUSE_BASE=((0x100+256)|MP_NO_REPEAT_KEY),
    MOUSE_BTN0=(MOUSE_BASE+0),
    MOUSE_BTN1=(MOUSE_BASE+1),
    MOUSE_BTN2=(MOUSE_BASE+2),
    MOUSE_BTN3=(MOUSE_BASE+3),
    MOUSE_BTN4=(MOUSE_BASE+4),
    MOUSE_BTN5=(MOUSE_BASE+5),
    MOUSE_BTN6=(MOUSE_BASE+6),
    MOUSE_BTN7=(MOUSE_BASE+7),
    MOUSE_BTN8=(MOUSE_BASE+8),
    MOUSE_BTN9=(MOUSE_BASE+9)
};
enum {
    MOUSE_BASE_DBL=(0x300|MP_NO_REPEAT_KEY),
    MOUSE_BTN0_DBL=(MOUSE_BASE_DBL+0),
    MOUSE_BTN1_DBL=(MOUSE_BASE_DBL+1),
    MOUSE_BTN2_DBL=(MOUSE_BASE_DBL+2),
    MOUSE_BTN3_DBL=(MOUSE_BASE_DBL+3),
    MOUSE_BTN4_DBL=(MOUSE_BASE_DBL+4),
    MOUSE_BTN5_DBL=(MOUSE_BASE_DBL+5),
    MOUSE_BTN6_DBL=(MOUSE_BASE_DBL+6),
    MOUSE_BTN7_DBL=(MOUSE_BASE_DBL+7),
    MOUSE_BTN8_DBL=(MOUSE_BASE_DBL+8),
    MOUSE_BTN9_DBL=(MOUSE_BASE_DBL+9)
};
#endif