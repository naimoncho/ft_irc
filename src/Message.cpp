#include "Message.hpp"
#include "Utils.hpp"

Message::Message(const std::string &line)
{
	size_t	i = 0;
	size_t	len = line.size();

	// A client may send a prefix; servers ignore it.
	if (i < len && line[i] == ':')
	{
		while (i < len && line[i] != ' ')
			++i;
	}
	while (i < len && line[i] == ' ')
		++i;

	size_t start = i;
	while (i < len && line[i] != ' ')
		++i;
	command = toUpper(line.substr(start, i - start));

	while (i < len)
	{
		while (i < len && line[i] == ' ')
			++i;
		if (i >= len)
			break;
		if (line[i] == ':')
		{
			params.push_back(line.substr(i + 1));
			break;
		}
		start = i;
		while (i < len && line[i] != ' ')
			++i;
		params.push_back(line.substr(start, i - start));
	}
}
