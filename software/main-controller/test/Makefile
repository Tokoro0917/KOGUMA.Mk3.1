# App/ の PC 上でのテスト（ToraNeko.Mk3 の software/main-controller/test/Makefile と同じ形）
#   make        ビルドしてテストを実行する
#   make clean  ビルドしたものを消す
#
#   test_maze  迷路（App/maze/）。探索走行・最短走行（App/sys/explore.c・shortest.c）は、ステップ9・10 で足す
#   test_wall_dist  壁センサの値を距離 [mm] に直す処理（App/ctrl/wall_dist.c）

APP_DIR = ../KOGUMA.Mk3.1/App

CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g -I$(APP_DIR)

# App/ のヘッダ全部。どれかを書き換えたら、全部のテストを作り直す（古いテストのまま通ってしまわないように）
APP_HEADERS = $(wildcard $(APP_DIR)/*/*.h)

MAZE_SOURCES = \
	$(APP_DIR)/maze/maze_map.c \
	$(APP_DIR)/maze/search.c \
	$(APP_DIR)/maze/path.c

test: test_maze test_wall_dist
	./test_wall_dist
	./test_maze

# dijkstra.c は test_maze.c の中で #include している（テストで中の関数を使うため）
test_maze: test_maze.c $(MAZE_SOURCES) $(APP_DIR)/maze/dijkstra.c $(APP_HEADERS)
	$(CC) $(CFLAGS) -o $@ test_maze.c $(MAZE_SOURCES) -lm

test_wall_dist: test_wall_dist.c $(APP_DIR)/ctrl/wall_dist.c $(APP_HEADERS)
	$(CC) $(CFLAGS) -o $@ test_wall_dist.c $(APP_DIR)/ctrl/wall_dist.c -lm

clean:
	rm -f test_maze test_wall_dist

.PHONY: test clean
