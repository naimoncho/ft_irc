NAME		= ircserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98
CPPFLAGS	= -Iincludes -MMD -MP

SRC_DIR		= src
OBJ_DIR		= obj

SRCS		= main.cpp \
			  Server.cpp \
			  Client.cpp \
			  Channel.cpp \
			  Message.cpp \
			  Utils.cpp \
			  commands/Registration.cpp \
			  commands/Channels.cpp \
			  commands/Messaging.cpp \
			  commands/Operators.cpp \
			  commands/Mode.cpp

OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))
DEPS		= $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
