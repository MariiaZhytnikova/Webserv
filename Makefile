
GREEN	:= \033[1;32m
YELLOW	:= \033[1;33m
BLUE	:= \033[1;34m
RESET	:= \033[0m
CYAN	:= \033[36m

NAME := webServ

SRC_DIR := ./srcs
INC_DIR := ./inc
OBJ_DIR := ./obj
LOG_DIR := ./log/

COUNT_FILE := .count

SRC := \
		$(SRC_DIR)/CgiHandler.cpp \
		$(SRC_DIR)/ConfigParser.cpp \
		$(SRC_DIR)/HttpRequest.cpp \
		$(SRC_DIR)/HttpResponse.cpp \
		$(SRC_DIR)/Location.cpp \
		$(SRC_DIR)/Logger.cpp \
		$(SRC_DIR)/main.cpp \
		$(SRC_DIR)/RequestHandler.cpp \
		$(SRC_DIR)/RequestValidator.cpp \
		$(SRC_DIR)/Server.cpp \
		$(SRC_DIR)/ServerManager.cpp \
		$(SRC_DIR)/Session.cpp \
		$(SRC_DIR)/SessionManager.cpp \
		$(SRC_DIR)/StaticDelete.cpp \
		$(SRC_DIR)/StaticGet.cpp \
		$(SRC_DIR)/StaticPost.cpp \
		$(SRC_DIR)/utils.cpp
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

TOTAL := $(words $(OBJ))

CC := c++
CFLAGS := -Wall -Wextra -Werror -std=c++20 -pedantic -I$(INC_DIR)

all: reset_count $(NAME)
	@./webServ

$(NAME): $(OBJ) $(LOG_DIR)
	@$(CC) $(CFLAGS) $(OBJ) -o $(NAME)
	@colors="31 33 32 36 34 35"; \
	for i in $$(seq 1 12); do \
		for c in $$colors; do \
			printf "\r\033[$${c}m>>> Starting webServ... <<<\033[0m"; \
			sleep 0.1; \
		done; \
	done; \
	printf "\n";

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
	@count=$$(cat $(COUNT_FILE)); \
	count=$$((count + 1)); \
	echo $$count > $(COUNT_FILE); \
	percent=$$((count * 100 / $(TOTAL))); \
	printf "\r$(CYAN)Building webServ...$(RESET)   $(YELLOW)[%3d%%]$(RESET)" $$percent; \
	if [ $$count -eq $(TOTAL) ]; then printf "\n"; fi

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(LOG_DIR):
	@mkdir -p $(LOG_DIR)

-include $(DEP)

reset_count:
	@echo 0 > $(COUNT_FILE)


clean:
	rm -rf $(OBJ_DIR) $(LOG_DIR)
	rm -f $(COUNT_FILE)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
