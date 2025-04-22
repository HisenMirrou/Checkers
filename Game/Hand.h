#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для обработки пользовательского ввода (мышь, кнопки)
class Hand
{
public:
    // Инициализация обработчика с привязкой к игровой доске
    Hand(Board *board) : board(board)
    {
    }

    // Обрабатывает клик мыши и возвращает:
    // - тип действия (выбор клетки/кнопки/выход)
    // - координаты выбранной клетки (если применимо)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;  // Событие SDL
        Response resp = Response::OK; // Результат обработки
        int x = -1, y = -1;    // Экранные координаты клика
        int xc = -1, yc = -1;  // Координаты на игровой доске

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT: // Обработка закрытия окна
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN: // Обработка клика мыши
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // Преобразование экранных координат в клетки доски
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Проверка нажатия кнопки "Назад"
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Проверка нажатия кнопки "Рестарт"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Проверка клика в пределах игрового поля
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    else // Клик вне игровой области
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT: // Обработка изменения размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }

                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};
    }

    // Ожидает действия пользователя (используется для финального экрана)
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT: // Закрытие окна
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: // Изменение размера
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: // Обработка клика
                {
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // Проверка нажатия кнопки "Рестарт"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }

                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

private:
    Board* board; // Указатель на игровую доску (для расчета координат)
};