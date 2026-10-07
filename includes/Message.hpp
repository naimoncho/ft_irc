#ifndef MESSAGE_HPP
# define MESSAGE_HPP

# include <string>
# include <vector>

// One parsed IRC line:   [:prefix] COMMAND param1 param2 ... [:trailing]
// The trailing parameter (the only one that may contain spaces) is stored
// as the last element of params, without its leading ':'.
struct Message
{
	std::string					command;
	std::vector<std::string>	params;

	explicit Message(const std::string &line);
};

#endif
