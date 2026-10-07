#include "Utils.hpp"

#include <cctype>
#include <sstream>

std::string toLower(const std::string &s)
{
	std::string out(s);

	for (size_t i = 0; i < out.size(); ++i)
		out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
	return out;
}

std::string toUpper(const std::string &s)
{
	std::string out(s);

	for (size_t i = 0; i < out.size(); ++i)
		out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));
	return out;
}

std::vector<std::string> split(const std::string &s, char sep)
{
	std::vector<std::string>	parts;
	size_t						start = 0;
	size_t						pos;

	while ((pos = s.find(sep, start)) != std::string::npos)
	{
		parts.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
	parts.push_back(s.substr(start));
	return parts;
}

std::string toString(long n)
{
	std::ostringstream oss;

	oss << n;
	return oss.str();
}

static bool isNickSpecial(char c)
{
	return std::string("[]\\`_^{|}").find(c) != std::string::npos;
}

// A nickname starts with a letter or a special character, continues with
// letters, digits, specials or '-', and is at most 30 characters long.
bool isValidNick(const std::string &nick)
{
	if (nick.empty() || nick.size() > 30)
		return false;
	for (size_t i = 0; i < nick.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(nick[i]);

		if (std::isalpha(c) || isNickSpecial(nick[i]))
			continue;
		if (i > 0 && (std::isdigit(c) || c == '-'))
			continue;
		return false;
	}
	return true;
}

// A channel name starts with '#', has at least one more character, and
// contains no space, comma or control character.
bool isValidChannelName(const std::string &name)
{
	if (name.size() < 2 || name.size() > 50 || name[0] != '#')
		return false;
	for (size_t i = 1; i < name.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(name[i]);

		if (c <= ' ' || c == ',' || c == 0x7f)
			return false;
	}
	return true;
}
