#include "Server.hpp"
#include "Utils.hpp"

static const size_t MAX_TOPIC = 300;

// JOIN <chan>{,<chan>} [<key>{,<key>}]      JOIN 0 leaves every channel
void Server::cmdJoin(Client &client, const Message &msg)
{
	if (msg.params.empty() || msg.params[0].empty())
	{
		reply(client, "461", "JOIN :Not enough parameters");
		return;
	}
	if (msg.params[0] == "0")
	{
		std::map<std::string, Channel *>::iterator it = _channels.begin();

		while (it != _channels.end())
		{
			Channel *channel = it->second;

			++it;
			if (channel->hasMember(&client))
			{
				channel->broadcast(":" + client.prefix() + " PART " + channel->getName());
				leaveChannel(client, channel);
			}
		}
		return;
	}

	std::vector<std::string> names = split(msg.params[0], ',');
	std::vector<std::string> keys;

	if (msg.params.size() > 1)
		keys = split(msg.params[1], ',');
	for (size_t i = 0; i < names.size(); ++i)
	{
		if (!names[i].empty())
			joinChannel(client, names[i], i < keys.size() ? keys[i] : std::string(""));
	}
}

void Server::joinChannel(Client &client, const std::string &name, const std::string &key)
{
	if (!isValidChannelName(name))
	{
		reply(client, "403", name + " :No such channel");
		return;
	}

	Channel *channel = findChannel(name);
	if (channel == NULL)
	{
		// The first user to join creates the channel and operates it.
		channel = new Channel(name);
		try
		{
			_channels[toLower(name)] = channel;
		}
		catch (...)
		{
			delete channel;
			throw;
		}
		channel->addMember(&client);
		channel->setOperator(&client, true);
	}
	else
	{
		if (channel->hasMember(&client))
			return;
		if (channel->isInviteOnly() && !channel->isInvited(&client))
		{
			reply(client, "473", channel->getName() + " :Cannot join channel (+i)");
			return;
		}
		if (!channel->getKey().empty() && channel->getKey() != key)
		{
			reply(client, "475", channel->getName() + " :Cannot join channel (+k)");
			return;
		}
		if (channel->isFull())
		{
			reply(client, "471", channel->getName() + " :Cannot join channel (+l)");
			return;
		}
		channel->addMember(&client);
	}

	channel->broadcast(":" + client.prefix() + " JOIN " + channel->getName());
	if (!channel->getTopic().empty())
		reply(client, "332", channel->getName() + " :" + channel->getTopic());
	sendNames(client, *channel);
}

void Server::sendNames(Client &client, const Channel &channel) const
{
	reply(client, "353", "= " + channel.getName() + " :" + channel.namesList());
	reply(client, "366", channel.getName() + " :End of /NAMES list");
}

// PART <chan>{,<chan>} [:reason]
void Server::cmdPart(Client &client, const Message &msg)
{
	if (msg.params.empty() || msg.params[0].empty())
	{
		reply(client, "461", "PART :Not enough parameters");
		return;
	}

	std::vector<std::string> names = split(msg.params[0], ',');
	for (size_t i = 0; i < names.size(); ++i)
	{
		Channel *channel = findChannel(names[i]);

		if (channel == NULL)
		{
			reply(client, "403", names[i] + " :No such channel");
			continue;
		}
		if (!channel->hasMember(&client))
		{
			reply(client, "442", channel->getName() + " :You're not on that channel");
			continue;
		}

		std::string line = ":" + client.prefix() + " PART " + channel->getName();
		if (msg.params.size() > 1 && !msg.params[1].empty())
			line += " :" + msg.params[1];
		channel->broadcast(line);
		leaveChannel(client, channel);
	}
}

// TOPIC <chan>            shows the topic
// TOPIC <chan> :<text>    changes it (an empty text clears it)
void Server::cmdTopic(Client &client, const Message &msg)
{
	if (msg.params.empty())
	{
		reply(client, "461", "TOPIC :Not enough parameters");
		return;
	}

	Channel *channel = findChannel(msg.params[0]);
	if (channel == NULL)
	{
		reply(client, "403", msg.params[0] + " :No such channel");
		return;
	}
	if (!channel->hasMember(&client))
	{
		reply(client, "442", channel->getName() + " :You're not on that channel");
		return;
	}
	if (msg.params.size() == 1)
	{
		if (channel->getTopic().empty())
			reply(client, "331", channel->getName() + " :No topic is set");
		else
			reply(client, "332", channel->getName() + " :" + channel->getTopic());
		return;
	}
	if (channel->isTopicRestricted() && !channel->isOperator(&client))
	{
		reply(client, "482", channel->getName() + " :You're not channel operator");
		return;
	}
	channel->setTopic(msg.params[1].substr(0, MAX_TOPIC));
	channel->broadcast(":" + client.prefix() + " TOPIC " + channel->getName() + " :" + channel->getTopic());
}

void Server::cmdNames(Client &client, const Message &msg)
{
	if (msg.params.empty())
	{
		reply(client, "366", "* :End of /NAMES list");
		return;
	}

	std::vector<std::string> names = split(msg.params[0], ',');
	for (size_t i = 0; i < names.size(); ++i)
	{
		Channel *channel = findChannel(names[i]);

		if (channel != NULL)
			sendNames(client, *channel);
		else
			reply(client, "366", names[i] + " :End of /NAMES list");
	}
}

// WHO is not required by the subject, but graphical clients send it right
// after joining a channel and expect at least the end-of-list reply.
void Server::cmdWho(Client &client, const Message &msg)
{
	std::string mask = msg.params.empty() ? "*" : msg.params[0];
	Channel		*channel = findChannel(mask);

	if (channel != NULL)
	{
		const std::set<Client *> &members = channel->getMembers();

		for (std::set<Client *>::const_iterator it = members.begin(); it != members.end(); ++it)
		{
			Client *member = *it;

			reply(client, "352", channel->getName() + " " + member->getUser() + " " + member->getHost()
				+ " " + SERVER_NAME + " " + member->getNick()
				+ (channel->isOperator(member) ? " H@" : " H") + " :0 " + member->getRealName());
		}
	}
	else
	{
		Client *target = findClient(mask);

		if (target != NULL && target->isRegistered())
			reply(client, "352", "* " + target->getUser() + " " + target->getHost() + " "
				+ SERVER_NAME + " " + target->getNick() + " H :0 " + target->getRealName());
	}
	reply(client, "315", mask + " :End of /WHO list");
}
