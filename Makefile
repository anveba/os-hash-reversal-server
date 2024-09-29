CC := gcc
FLAGS := -DSB_VERBOSE -Wall -std=gnu11
DEBUG_FLAGS = -p -g3
RELEASE_FLAGS = -O3 -flto -DNDEBUG
INCLUDE := -Isrc -Ivendor
LINK := -lpthread # -lcrypto -lssl

CCFLAGS := $(FLAGS) $(INCLUDE)
LDFLAGS := $(FLAGS)

BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src

TARGET_NAME := server
TARGET := $(TARGET_NAME)

SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

CLEAN_LIST := $(TARGET) $(OBJ)

default: makedir all

.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH)

.PHONY: all
all: 
	$(CC) -o $(TARGET) $(SRC) $(CCFLAGS) $(LINK) $(RELEASE_FLAGS)

.PHONY: debug
debug: 
	$(CC) -o $(TARGET) $(SRC) $(CCFLAGS) $(LINK) $(DEBUG_FLAGS)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -rf $(CLEAN_LIST)