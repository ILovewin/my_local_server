# 设置交叉编译工具链路径
TOOLCHAIN_PATH := /opt/tools-master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin

# 默认编译器为本地 gcc
CC := gcc

# 如果传入了 CROSS_COMPILE 参数，则使用交叉编译工具链
ifdef CROSS_COMPILE
    CC := $(TOOLCHAIN_PATH)/arm-linux-gnueabihf-gcc
endif

# 编译选项
CFLAGS := -Wall -Wextra -g -pthread -I./include -std=c99

# 源文件目录
SRC_DIR := src

# 目标文件目录
OBJ_DIR := obj

# 所有源文件
SRCS := $(wildcard $(SRC_DIR)/*.c)

# 所有目标文件
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# 默认目标
all: server client

# 确保 obj 目录存在
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# 生成 server 可执行文件
server: $(OBJS)
	$(CC) $(CFLAGS) -o server $(OBJ_DIR)/server.o $(OBJ_DIR)/lst_timer.o

# 生成 client 可执行文件
client: $(OBJ_DIR)/client.o
	$(CC) $(CFLAGS) -o client $(OBJ_DIR)/client.o

# 编译所有源文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# 清理生成的文件
clean:
	rm -f server client $(OBJS)
