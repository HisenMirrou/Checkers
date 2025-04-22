#pragma once
#include <stdlib.h>

typedef int8_t POS_T;

// Структура, представляющая ход в игре
struct move_pos
{
    POS_T x, y;             // Начальная позиция
    POS_T x2, y2;           // Конечная позиция 
    POS_T xb = -1, yb = -1; // Съеденная фигура (-1, если фигура не бьет)

    // Ход без взятия фигуры
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
        : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Ход со взятием фигуры (указывается позиция побитой фигуры)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Оператор сравнения (ходы считаются равными, если начальная и конечная позиции совпадают)
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }

    // Оператор неравенства (если ходы не равны)
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};