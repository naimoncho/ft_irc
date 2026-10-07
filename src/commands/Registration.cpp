#include "Server.hpp"
#include "Utils.hpp"

#include <ctime>

// CAP: capability negotiation. Modern clients (irssi, HexChat...) open with
// "CAP LS" and wait for an answer before going on. This server supports no
// capability, so it answers with an empty list and refuses every request.
void Server::cmdCap(Client &client, const Message &msg)
{
	std::string nick = client.getNick().empty() ? "*" : client.getNick();
	std::string head = std::string(":") + SERVER_NAME + " CAP " + nick + " ";

	if (msg.params.empty())
		return;

	std::string sub = toUpper(msg.params[0]);
	if (sub == "LS")
		client.queue(head + "LS :");
	else if (sub == "LIST")
		client.queue(head + "LIST :");
	else if (sub == "REQ")
		client.queue(head + "NAK :" + (msg.params.size() > 1 ? msg.params[1] : std::string("")));
}

void Server::cmdPass(Client &client, const Message &msg)
{
	if (client.isRegistered())
	{
		reply(client, "462", ":You may not reregister");
		return;
	}
	if (msg.params.empty())
	{
		reply(client, "461", "PASS :Not enough parameters");
		return;
	}
	client.setPassword(msg.params[0] == _password);
	if (!client.hasPassword())
		reply(client, "464", ":Password incorrect");
}

void Server::cmdNick(Client &client, const Message &msg)
{
	if (msg.params.empty() || msg.params[0].empty())
	{
		reply(client, "431", ":No nickname given");
		return;
	}

	const std::string &nick = msg.params[0];
	if (!isValidNick(nick))
	{
		reply(client, "432", nick + " :Erroneous nickname");
		return;
	}

	Client *owner = findClient(nick);
	if (owner != NULL && owner != &client)
	{
		reply(client, "433", nick + " :Nickname is already in use");
		return;
	}
	if (nick == client.getNick())
		return;

	if (client.isRegistered())
	{
		// Everyone who can see this user must learn the new name; the
		// line still carries the old one as its source.
		broadcastToPeers(client, ":" + client.prefix() + " NICK :" + nick, true);
		client.setNick(nick);
		return;
	}
	client.setNick(nick);
	tryRegister(client);
}

void Server::cmdUser(Client &client, const Message &msg)
{
	if (client.isRegistered())
	{
		reply(client, "462", ":You may not reregister");
		return;
	}
	if (msg.params.size() < 4 || msg.params[0].empty())
	{
		reply(client, "461", "USER :Not enough parameters");
		return;
	}
	client.setUser(msg.params[0], msg.params[3]);
	tryRegister(client);
}

// Registration completes once both NICK and USER were received, in any
// order. The password is checked here: without the right PASS before that
// point the connection is refused.
void Server::tryRegister(Client &client)
{
	if (client.isRegistered() || client.getNick().empty() || client.getUser().empty())
		return;
	if (!client.hasPassword())
	{
		reply(client, "464", ":Password incorrect");
		disconnect(client, "Bad password", true);
		return;
	}
	client.setRegistered();

	reply(client, "001", ":Welcome to the Internet Relay Network " + client.prefix());
	reply(client, "002", std::string(":Your host is ") + SERVER_NAME + ", running version 1.0");
	reply(client, "003", ":This server was created for the 42 ft_irc project");
	reply(client, "004", std::string(SERVER_NAME) + " 1.0 o itkol");
	reply(client, "005", "CHANTYPES=# PREFIX=(o)@ CHANMODES=,k,l,it NICKLEN=30 CHANNELLEN=50"
		" CASEMAPPING=ascii :are supported by this server");
	reply(client, "422", ":MOTD File is missing");
}

void Server::cmdPing(Client &client, const Message &msg)
{
	if (msg.params.empty())
	{
		reply(client, "409", ":No origin specified");
		return;
	}
	client.queue(std::string(":") + SERVER_NAME + " PONG " + SERVER_NAME + " :" + msg.params[0]);
}

// The server never sends PING itself, so a PONG carries no information.
void Server::cmdPong(Client &, const Message &)
{
}

void Server::cmdQuit(Client &client, const Message &msg)
{
	std::string reason = "Quit";

	if (!msg.params.empty() && !msg.params[0].empty())
		reason = "Quit: " + msg.params[0];
	disconnect(client, reason, true);
}
