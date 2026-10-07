#include "Server.hpp"
#include "Utils.hpp"

#include <cstdlib>

// MODE <chan>                       shows the channel modes
// MODE <chan> <modestring> [args]   changes them (operators only)
// MODE <nick> ...                   user modes: none are supported
void Server::cmdMode(Client &client, const Message &msg)
{
	if (msg.params.empty())
	{
		reply(client, "461", "MODE :Not enough parameters");
		return;
	}

	const std::string &target = msg.params[0];
	if (target[0] != '#')
	{
		// Clients set user modes on themselves right after connecting
		// ("MODE nick +i"). There is nothing to do, but it is not an error.
		Client *user = findClient(target);

		if (user == NULL)
			reply(client, "401", target + " :No such nick/channel");
		else if (user != &client)
			reply(client, "502", ":Cannot change mode for other users");
		else if (msg.params.size() == 1)
			reply(client, "221", "+");
		return;
	}

	Channel *channel = findChannel(target);
	if (channel == NULL)
	{
		reply(client, "403", target + " :No such channel");
		return;
	}
	if (msg.params.size() == 1)
	{
		reply(client, "324", channel->getName() + " " + channel->modeString(channel->hasMember(&client)));
		reply(client, "329", channel->getName() + " " + toString(static_cast<long>(channel->getCreationTime())));
		return;
	}
	// Clients ask for the ban list when they join ("MODE #chan b"). Bans
	// do not exist here, so the list is always empty.
	if (msg.params[1] == "b" || msg.params[1] == "+b")
	{
		reply(client, "368", channel->getName() + " :End of channel ban list");
		return;
	}
	if (!channel->hasMember(&client))
	{
		reply(client, "442", channel->getName() + " :You're not on that channel");
		return;
	}
	if (!channel->isOperator(&client))
	{
		reply(client, "482", channel->getName() + " :You're not channel operator");
		return;
	}
	applyChannelModes(client, *channel, msg);
}

// How many arguments the rest of a modestring will consume. Used to decide
// whether "-k" was given its (optional) key argument.
static size_t argsNeeded(const std::string &modes, size_t from, bool adding)
{
	size_t count = 0;

	for (size_t i = from; i < modes.size(); ++i)
	{
		if (modes[i] == '+')
			adding = true;
		else if (modes[i] == '-')
			adding = false;
		else if (modes[i] == 'o' || (adding && (modes[i] == 'k' || modes[i] == 'l')))
			++count;
	}
	return count;
}

// Records one mode change that really happened, building the line that is
// broadcast to the channel, e.g. "+kl-i secret 10".
static void record(std::string &changes, std::string &args, char &lastSign,
	bool adding, char mode, const std::string &arg)
{
	char sign = adding ? '+' : '-';

	if (sign != lastSign)
	{
		changes += sign;
		lastSign = sign;
	}
	changes += mode;
	if (!arg.empty())
		args += " " + arg;
}

// A limit is a positive decimal number that fits comfortably in a long.
static bool parseLimit(const std::string &arg, size_t &limit)
{
	if (arg.empty() || arg.size() > 6)
		return false;
	for (size_t i = 0; i < arg.size(); ++i)
	{
		if (arg[i] < '0' || arg[i] > '9')
			return false;
	}
	limit = static_cast<size_t>(std::atol(arg.c_str()));
	return limit > 0;
}

// Walks the modestring one character at a time. '+' and '-' switch
// between adding and removing; modes that take an argument consume the
// next unused parameter. Each flag is paired with its own argument, so
// "+kl-i secret 10" or "+o-o alice bob" work as expected.
void Server::applyChannelModes(Client &client, Channel &channel, const Message &msg)
{
	const std::string	&modes = msg.params[1];
	size_t				next = 2;
	bool				adding = true;
	std::string			changes;
	std::string			args;
	char				lastSign = 0;

	for (size_t i = 0; i < modes.size(); ++i)
	{
		char mode = modes[i];

		if (mode == '+' || mode == '-')
		{
			adding = (mode == '+');
			continue;
		}
		if (mode == 'i')
		{
			if (channel.isInviteOnly() != adding)
			{
				channel.setInviteOnly(adding);
				record(changes, args, lastSign, adding, mode, "");
			}
		}
		else if (mode == 't')
		{
			if (channel.isTopicRestricted() != adding)
			{
				channel.setTopicRestricted(adding);
				record(changes, args, lastSign, adding, mode, "");
			}
		}
		else if (mode == 'k' && adding)
		{
			if (next >= msg.params.size())
			{
				reply(client, "461", "MODE :Not enough parameters");
				continue;
			}

			const std::string &key = msg.params[next++];
			if (key.empty() || key.find(',') != std::string::npos)
			{
				reply(client, "525", channel.getName() + " :Key is not well-formed");
				continue;
			}
			channel.setKey(key);
			record(changes, args, lastSign, adding, mode, key);
		}
		else if (mode == 'k')
		{
			// "-k" and "-k <key>" are both accepted: the argument is only
			// taken when it is not needed by a later flag.
			if (msg.params.size() - next > argsNeeded(modes, i + 1, adding))
				++next;
			if (!channel.getKey().empty())
			{
				channel.setKey("");
				record(changes, args, lastSign, adding, mode, "*");
			}
		}
		else if (mode == 'l' && adding)
		{
			size_t limit;

			if (next >= msg.params.size())
			{
				reply(client, "461", "MODE :Not enough parameters");
				continue;
			}

			const std::string &arg = msg.params[next++];
			if (!parseLimit(arg, limit))
			{
				reply(client, "696", channel.getName() + " l " + arg + " :Invalid limit");
				continue;
			}
			channel.setLimit(limit);
			record(changes, args, lastSign, adding, mode, toString(static_cast<long>(limit)));
		}
		else if (mode == 'l')
		{
			if (channel.getLimit() != 0)
			{
				channel.setLimit(0);
				record(changes, args, lastSign, adding, mode, "");
			}
		}
		else if (mode == 'o')
		{
			if (next >= msg.params.size())
			{
				reply(client, "461", "MODE :Not enough parameters");
				continue;
			}

			const std::string	&nick = msg.params[next++];
			Client				*target = findClient(nick);

			if (target == NULL)
			{
				reply(client, "401", nick + " :No such nick/channel");
				continue;
			}
			if (!channel.hasMember(target))
			{
				reply(client, "441", target->getNick() + " " + channel.getName() + " :They aren't on that channel");
				continue;
			}
			if (channel.isOperator(target) != adding)
			{
				channel.setOperator(target, adding);
				record(changes, args, lastSign, adding, mode, target->getNick());
			}
		}
		else
			reply(client, "472", std::string(1, mode) + " :is unknown mode char to me");
	}
	if (!changes.empty())
		channel.broadcast(":" + client.prefix() + " MODE " + channel.getName() + " " + changes + args);
}
