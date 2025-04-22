#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        // Очищаем лог-файл при создании игры
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Основной метод запуска игры
    int play()
    {
        auto start = chrono::steady_clock::now(); // Фиксируем время начала игры

        // Обработка режима повтора игры
        if (is_replay)
        {
            logic = Logic(&board, &config);  // Пересоздаем логику
            config.reload();                 // Обновляем конфигурацию
            board.redraw();                  // Перерисовываем доску
        }
        else
        {
            board.start_draw();  // Первоначальная отрисовка доски
        }

        is_replay = false; // Сбрасываем флаг повтора

        int turn_num = -1;      // Счетчик ходов
        bool is_quit = false;   // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns"); // Лимит ходов

        // Главный игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;  // Сбрасываем счетчик серии взятий

            // Определяем доступные ходы для текущего игрока
            logic.find_turns(turn_num % 2);

            // Если ходов нет - завершаем игру
            if (logic.turns.empty())
                break;

            // Устанавливаем сложность бота для текущего хода
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            // Обработка хода игрока или бота
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                auto resp = player_turn(turn_num % 2);

                if (resp == Response::QUIT)  // Выход из игры
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)  // Запрос на повтор
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)  // Отмена хода
                {
                    // Особый случай отмены при игре против бота
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }

                    // Стандартная отмена хода
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2);  // Ход бота
            }
        }

        auto end = chrono::steady_clock::now(); // Фиксируем время окончания

        // Логируем время игры
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Обработка запроса на повтор
        if (is_replay)
            return play();

        // Выход без завершения
        if (is_quit)
            return 0;

        // Определение результата игры
        int res = 2; // По умолчанию - незавершенная игра
        if (turn_num == Max_turns)
        {
            res = 0; // Ничья
        }
        else if (turn_num % 2)
        {
            res = 1; // Победа одного из игроков
        }

        board.show_final(res); // Отображение финального экрана

        // Ожидание реакции игрока
        auto resp = hand.wait();

        // Обработка запроса на повтор
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }

        return res; // Возврат результата
    }

private:
    // Обработка хода бота
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now(); // Время начала хода

        // Задержка между ходами бота
        auto delay_ms = config("Bot", "BotDelayMS");

        // Поток для реализации задержки
        thread th(SDL_Delay, delay_ms);

        // Поиск оптимальных ходов
        auto turns = logic.find_best_turns(color);

        th.join(); // Ожидание завершения задержки

        bool is_first = true;
        // Выполнение серии ходов
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);  // Задержка между ходами
            }
            is_first = false;

            // Учет взятия фигуры
            beat_series += (turn.xb != -1);

            // Выполнение хода на доске
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now(); // Время окончания хода

        // Логирование времени хода
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // Обработка хода игрока
    Response player_turn(const bool color)
    {
        // Подсветка доступных ходов
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells);

        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;

        // Цикл ожидания ввода игрока
        while (true)
        {
            auto resp = hand.get_cell();
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);

            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            // Проверка корректности хода
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }

            if (pos.x != -1)
                break;

            // Обработка некорректного хода
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }

            // Обновление интерфейса при корректном ходе
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);

            // Подсветка следующих возможных ходов
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }

        // Выполнение хода
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);

        // Завершение хода если не было взятия
        if (pos.xb == -1)
            return Response::OK;

        // Обработка серии взятий
        beat_series = 1;
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats)
                break;

            // Подсветка доступных взятий
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);

            // Ожидание выбора взятия
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);

                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }

                if (!is_correct)
                    continue;

                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }

        return Response::OK;
    }

private:
    Config config;        // Конфигурация игры
    Board board;          // Игровая доска
    Hand hand;            // Обработчик ввода
    Logic logic;          // Игровая логика
    int beat_series;      // Счетчик серии взятий
    bool is_replay = false; // Флаг повтора игры
}; 