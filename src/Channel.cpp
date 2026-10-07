#include "Channel.hpp"
#include "Client.hpp"
#include "Utils.hpp"

Channel::Channel(const std::string &name)
	: _name(name), _limit(0), _inviteOnly(false), _topicRestricted(false),
	  _created(std::time(NULL))
{
}

Channel::~Channel()
{
}

const std::string	&Channel::getName() const { return _name; }
std::time_t			Channel::getCreationTime() const { return _created; }
const std::string	&Channel::getTopic() const { return _topic; }
void				Channel::setTopic(const std::string &topic) { _topic = topic; }

const std::set<Client *> &Channel::getMembers() const { return _members; }

bool Channel::hasMember(Client *client) const
{
	return _members.find(client) != _members.end();
}

bool Channel::isOperator(Client *client) const
{
	return _operators.find(client) != _operators.end();
}

bool Channel::isEmpty() const { return _members.empty(); }

void Channel::addMember(Client *client)
{
	_members.insert(client);
	_invited.erase(client);
}

// Also forgets operator status and any pending invitation, so that nothing
// in the channel refers to the client afterwards.
void Channel::removeMember(Client *client)
{
	_members.erase(client);
	_operators.erase(client);
	_invited.erase(client);
}

void Channel::setOperator(Client *client, bool on)
{
	if (on && hasMember(client))
		_operators.insert(client);
	else if (!on)
		_operators.erase(client);
}

bool				Channel::isInviteOnly() const { return _inviteOnly; }
void				Channel::setInviteOnly(bool on) { _inviteOnly = on; }
bool				Channel::isTopicRestricted() const { return _topicRestricted; }
void				Channel::setTopicRestricted(bool on) { _topicRestricted = on; }
const std::string	&Channel::getKey() const { return _key; }
void				Channel::setKey(const std::string &key) { _key = key; }
size_t				Channel::getLimit() const { return _limit; }
void				Channel::setLimit(size_t limit) { _limit = limit; }

bool Channel::isFull() const
{
	return _limit != 0 && _members.size() >= _limit;
}

bool Channel::isInvited(Client *client) const
{
	return _invited.find(client) != _invited.end();
}

void Channel::addInvite(Client *client) { _invited.insert(client); }

void Channel::broadcast(const std::string &line, Client *except) const
{
	for (std::set<Client *>::const_iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it != except)
			(*it)->queue(line);
	}
}

std::string Channel::namesList() const
{
	std::string list;

	for (std::set<Client *>::const_iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (!list.empty())
			list += " ";
		if (isOperator(*it))
			list += "@";
		list += (*it)->getNick();
	}
	return list;
}

std::string Channel::modeString(bool showKey) const
{
	std::string modes = "+";
	std::string args;

	if (_inviteOnly)
		modes += "i";
	if (_topicRestricted)
		modes += "t";
	if (!_key.empty())
	{
		modes += "k";
		args += " " + (showKey ? _key : std::string("*"));
	}
	if (_limit != 0)
	{
		modes += "l";
		args += " " + toString(static_cast<long>(_limit));
	}
	return modes + args;
}
