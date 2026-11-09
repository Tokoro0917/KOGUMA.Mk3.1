#!/usr/bin/env python3
"""KOGUMA.MkIIl の迷路の出力（MAZE_DUMP_BEGIN 〜 MAZE_DUMP_END の行）を、テスト用の迷路のテキストに直す。

KOGUMA.MkIIl の mazes/ の迷路や、KOGUMA.MkIIl のモード3 No.0・モード4 No.7 の出力を、
test_maze.c で読める形（gen_maze.py と同じ形）にするためのもの。

KOGUMA の出力の形（KOGUMA.MkIIl の Core/Src/Maze.c の Maze_Debug_Dump）:
    ROW y  16進    横の線 y（区画 (x, y-1) と (x, y) のあいだ）の壁。x 番目の位（1 の位が x=0）が 1 なら壁
    COL x  16進    縦の線 x（区画 (x-1, y) と (x, y) のあいだ）の壁。y 番目の位が 1 なら壁
    LROW・LCOL     その壁を見たか（ここでは使わない。全部の壁がわかっている迷路だけを直す）

使い方:  python3 from_koguma_dump.py 入力ファイル 出力ファイル [ゴールの x y 幅 高さ]
    ゴールを書かなければ、16×16 の中央の 2×2（7, 7, 2, 2）にする
"""
import sys

SIZE = 16


def read_dump(path):
    rows = {}
    cols = {}
    inside = False
    with open(path, encoding="utf-8") as f:
        for line in f:
            words = line.split()
            if not words:
                continue
            if words[0] == "MAZE_DUMP_BEGIN":
                inside = True
                continue
            if words[0] == "MAZE_DUMP_END":
                break
            if not inside:
                continue
            if words[0] == "ROW":
                rows[int(words[1])] = int(words[2], 16)
            elif words[0] == "COL":
                cols[int(words[1])] = int(words[2], 16)
    return rows, cols


def has_bit(value, index):
    # value の index 番目の位が 1 か（1 の位が 0 番目）
    return (value // (2 ** index)) % 2 == 1


def to_text(rows, cols, goal):
    goal_x, goal_y, goal_w, goal_h = goal

    def in_goal(x, y):
        return goal_x <= x < goal_x + goal_w and goal_y <= y < goal_y + goal_h

    lines = []
    for y in range(SIZE - 1, -1, -1):
        # 区画 y の北側の壁の行（横の線 y+1）
        row = "o"
        for x in range(SIZE):
            if has_bit(rows[y + 1], x):
                row += "---o"
            else:
                row += "   o"
        lines.append(row)
        # 区画 y の行（西の外周 + 各区画の中身と東の壁）
        row = "|"
        for x in range(SIZE):
            if x == 0 and y == 0:
                mark = "S"
            elif in_goal(x, y):
                mark = "G"
            else:
                mark = " "
            row += " " + mark + " "
            if has_bit(cols[x + 1], y):
                row += "|"
            else:
                row += " "
        lines.append(row)
    lines.append("o" + "---o" * SIZE)  # 南の外周
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) != 3 and len(sys.argv) != 7:
        print(__doc__)
        sys.exit(1)
    goal = (7, 7, 2, 2)
    if len(sys.argv) == 7:
        goal = tuple(int(v) for v in sys.argv[3:7])
    rows, cols = read_dump(sys.argv[1])
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write(to_text(rows, cols, goal))
    print("wrote", sys.argv[2])


main()
