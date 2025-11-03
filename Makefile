NAME := webServ

SRC_DIR := ./srcs
INC_DIR := ./inc
OBJ_DIR := ./obj
LOG_DIR := ./log/

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

CC := c++
CFLAGS := -Wall -Wextra -Werror -std=c++20 -pedantic -I$(INC_DIR)

all: $(NAME)

$(NAME): $(OBJ) $(LOG_DIR)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(LOG_DIR):
	@mkdir -p $(LOG_DIR)

-include $(DEP)

clean:
	rm -rf $(OBJ_DIR) $(LOG_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
