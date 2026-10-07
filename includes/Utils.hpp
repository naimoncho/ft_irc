#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <vector>

// ASCII-only case conversion: IRC nicknames and channel names are compared
// case-insensitively, so every lookup goes through toLower().
std::string					toLower(const std::string &s);
std::string					toUpper(const std::string &s);

// Splits on a separator, keeping empty fields ("a,,b" -> "a", "", "b").
std::vector<std::string>	split(const std::string &s, char sep);

std::string					toString(long n);

bool						isValidNick(const std::string &nick);
bool						isValidChannelName(const std::string &name);

#endif
