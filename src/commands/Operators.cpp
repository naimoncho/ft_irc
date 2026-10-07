#include "Server.hpp"
#include "Utils.hpp"

// KICK <chan> <nick>{,<nick>} [:reason]
void Server::cmdKick(Client &client, const Message &msg)
{
	if (msg.params.size() < 2 || msg.params[1].empty())
	{
		reply(client, "461", "KICK :Not enough parameters");
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
	if (!channel->isOperator(&client))
	{
		reply(client, "482", channel->getName() + " :You're not channel operator");
		return;
	}

	std::string reason = client.getNick();
	if (msg.params.size() > 2 && !msg.params[2].empty())
		reason = msg.params[2];

	// Kicking the last member destroys the channel, so its name is copied
	// and the pointer is looked up again on every turn.
	const std::string			name = channel->getName();
	std::vector<std::string>	nicks = split(msg.params[1], ',');

	for (size_t i = 0; i < nicks.size(); ++i)
	{
		channel = findChannel(name);
		if (channel == NULL)
			return;
		if (nicks[i].empty())
			continue;

		Client *target = findClient(nicks[i]);
		if (target == NULL)
		{
			reply(client, "401", nicks[i] + " :No such nick/channel");
			continue;
		}
		if (!channel->hasMember(target))
		{
			reply(client, "441", target->getNick() + " " + name + " :They aren't on that channel");
			continue;
		}
		// The kicked user is still a member here, so it sees the KICK too.
		channel->broadcast(":" + client.prefix() + " KICK " + name + " " + target->getNick() + " :" + reason);
		leaveChannel(*target, channel);
	}
}

// INVITE <nick> <chan>
// The invitation lets the target pass the +i check once; it is consumed
// when the target joins.
void Server::cmdInvite(Client &client, const Message &msg)
{
	if (msg.params.size() < 2)
	{
		reply(client, "461", "INVITE :Not enough parameters");
		return;
	}

	Client *target = findClient(msg.params[0]);
	if (target == NULL || !target->isRegistered())
	{
		reply(client, "401", msg.params[0] + " :No such nick/channel");
		return;
	}

	Channel *channel = findChannel(msg.params[1]);
	if (channel == NULL)
	{
		reply(client, "403", msg.params[1] + " :No such channel");
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
	if (channel->hasMember(target))
	{
		reply(client, "443", target->getNick() + " " + channel->getName() + " :is already on channel");
		return;
	}
	channel->addInvite(target);
	reply(client, "341", target->getNick() + " " + channel->getName());
	target->queue(":" + client.prefix() + " INVITE " + target->getNick() + " :" + channel->getName());
}
