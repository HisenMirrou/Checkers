#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
public:
    Logic(Board* board, Config* config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color)
    {
        // Очищаем вспомогательные структуры перед поиском ходов
        next_move.clear();
        next_best_state.clear();

        // Запускаем рекурсивный поиск оптимального хода для текущего цвета
        // Параметры: текущее состояние доски, цвет, стартовая позиция (-1,-1), глубина 0
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        // Строим цепочку лучших ходов из полученных данных
        vector<move_pos> res;
        int state = 0;

        // Последовательно собираем ходы, пока не достигнем конечного состояния (-1)
        // или пустого хода (x == -1)
        do {
            res.push_back(next_move[state]);
            state = next_best_state[state];
        } while (state != -1 && next_move[state].x != -1);

        return res;
    }
    
private:
    // делаем ход
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // подсчет состояния бота
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);  // количество белых фишек
                wq += (mtx[i][j] == 3); //            белых королев
                b += (mtx[i][j] == 2);  //            черных фишек
                bq += (mtx[i][j] == 4); //            черных королев
                if (scoring_mode == "NumberAndPotential") // подсчет очков для обычных фишек
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;
        if (b + bq == 0)
            return 0;
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef); // оценка состояния бота
    }


    // Рекурсивный поиск лучшего хода с возможностью продолжения серии взятий
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1)
    {
        // Инициализация структур для хранения информации о ходе
        next_move.emplace_back(-1, -1, -1, -1);  // Пустой ход по умолчанию
        next_best_state.push_back(-1);           // Конечное состояние по умолчанию

        // Поиск возможных ходов, если это не начальное состояние
        if (state != 0)
        {
            find_turns(x, y, mtx);
        }

        // Сохраняем текущие ходы и флаг наличия взятий
        auto now_turns = turns;
        bool now_have_beats = have_beats;

        // Если нет взятий и это не начальное состояние - передаем ход противнику
        if (!now_have_beats && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        double best_score = -1;  // Лучшая оценка хода

        // Перебор всех возможных ходов
        for (auto turn : now_turns)
        {
            size_t next_state = next_move.size();
            double score;

            // Если есть взятия - продолжаем ход той же фигурой
            if (now_have_beats)
            {
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else  // Иначе передаем ход противнику
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Обновляем информацию о лучшем ходе
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = (now_have_beats ? next_state : -1);
                next_move[state] = turn;
            }
        }

        return best_score;
    }

    // Рекурсивная функция поиска лучшего хода с альфа-бета отсечением
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Если достигнута максимальная глубина - оцениваем позицию
        if (depth == Max_depth)
        {
            return calc_score(mtx, (depth % 2 == color));
        }

        // Поиск ходов для конкретной фигуры после взятия
        if (x != -1 && y != -1)
        {
            find_turns(x, y, mtx);
        }
        else  // Иначе ищем все возможные ходы для текущего цвета
        {
            find_turns(color, mtx);
        }

        auto curTurns = turns;
        auto cur_have_beats = have_beats;

        // Если нет взятий и это продолжение хода - передаем ход противнику
        if (!cur_have_beats && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta, -1, -1);
        }

        // Если нет доступных ходов - это поражение
        if (curTurns.empty())
        {
            return (depth % 2 ? 0 : INF);
        }

        double min_score = INF + 1;  // Минимальная оценка для MIN-игрока
        double max_score = -1;       // Максимальная оценка для MAX-игрока

        // Перебор всех возможных ходов
        for (auto turn : curTurns)
        {
            double score = 0.0;

            // Если есть взятия - продолжаем ход той же фигурой
            if (cur_have_beats)
            {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            else  // Иначе передаем ход противнику
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }

            // Обновляем минимальную и максимальную оценки
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Альфа-бета отсечение
            if (depth % 2)
                alpha = max(alpha, max_score);
            else
                beta = min(beta, min_score);

            // Прекращаем перебор при выполнении условия отсечения
            if (optimization != "O0" && alpha > beta)
                break;

            // Дополнительное отсечение при равенстве альфа и бета
            if (optimization != "O2" && alpha == beta)
                return (depth % 2 ? max_score + 1 : min_score - 1);
        }

        // Возвращаем оптимальную оценку в зависимости от глубины
        return (depth % 2 ? max_score : min_score);
    }

public:
    // поиск возможных ходов для определенного цвета
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }
    // поиск определенного хода для клетки
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // основной метод для поиска ходов по цвету
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // тоже самое но для конкретной позиции
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1);
            for (POS_T j = y - 1; j <= y + 1; j += 2)
            {
                if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                    continue;
                turns.emplace_back(x, y, i, j);
            }
            break;
        }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

public:
    vector<move_pos> turns; // возможные ходы
    bool have_beats; // есть ли побитие
    int Max_depth; // максимальная глубина поиска

private:
    default_random_engine rand_eng; // генератор случайных чисел
    string scoring_mode; // режим подсчета очков
    string optimization; // оптимизация
    vector<move_pos> next_move; // лучшие ходы
    vector<int> next_best_state; // состояние после выполнения ходов
    Board* board; // указатель на доску
    Config* config;  // указатель на config
};