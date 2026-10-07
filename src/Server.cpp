#include "Server.hpp"
#include "Utils.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <ctime>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// Longest input a client may send without a line terminator. The protocol
// limit is 512 bytes per message; anything beyond this is abuse.
static const size_t	MAX_INPUT = 4096;
// How long a connection being closed is given to receive its last lines.
static const int	CLOSE_GRACE = 5;

volatile sig_atomic_t Server::_stop = 0;

void Server::stop()
{
	_stop = 1;
}

/* ************************************************************************** */
/*   Construction                                                             */
/* ************************************************************************** */

Server::Server(int port, const std::string &password)
	: _listenFd(-1), _password(password)
{
	_handlers["CAP"] = &Server::cmdCap;
	_handlers["PASS"] = &Server::cmdPass;
	_handlers["NICK"] = &Server::cmdNick;
	_handlers["USER"] = &Server::cmdUser;
	_handlers["PING"] = &Server::cmdPing;
	_handlers["PONG"] = &Server::cmdPong;
	_handlers["QUIT"] = &Server::cmdQuit;
	_handlers["JOIN"] = &Server::cmdJoin;
	_handlers["PART"] = &Server::cmdPart;
	_handlers["TOPIC"] = &Server::cmdTopic;
	_handlers["NAMES"] = &Server::cmdNames;
	_handlers["WHO"] = &Server::cmdWho;
	_handlers["PRIVMSG"] = &Server::cmdPrivmsg;
	_handlers["NOTICE"] = &Server::cmdNotice;
	_handlers["KICK"] = &Server::cmdKick;
	_handlers["INVITE"] = &Server::cmdInvite;
	_handlers["MODE"] = &Server::cmdMode;

	try
	{
		setupListener(port);
	}
	catch (...)
	{
		if (_listenFd != -1)
			close(_listenFd);
		throw;
	}
}

Server::~Server()
{
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
	for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
		delete it->second;
	if (_listenFd != -1)
		close(_listenFd);
}

void Server::setupListener(int port)
{
	struct sockaddr_in	addr;
	int					yes = 1;

	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("socket() failed");
	// Lets the server restart on the same port without waiting for the
	// kernel to release it.
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		throw std::runtime_error("setsockopt() failed");
	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl() failed");

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(static_cast<unsigned short>(port));
	if (bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
		throw std::runtime_error("bind() failed: is the port already in use?");
	if (listen(_listenFd, SOMAXCONN) < 0)
		throw std::runtime_error("listen() failed");
	std::cout << "Listening on port " << port << std::endl;
}

/* ************************************************************************** */
/*   Event loop                                                               */
/* ************************************************************************** */

// The poll set is rebuilt on every iteration from the client map. A client
// is only watched for POLLOUT while it has something queued, otherwise
// poll() would return immediately all the time.
void Server::buildPollFds(std::vector<struct pollfd> &fds) const
{
	struct pollfd pfd;

	fds.clear();
	pfd.fd = _listenFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	fds.push_back(pfd);
	for (std::map<int, Client *>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		const Client &client = *it->second;

		pfd.fd = it->first;
		pfd.events = 0;
		if (!client.isClosing())
			pfd.events |= POLLIN;
		if (client.hasPendingOutput())
			pfd.events |= POLLOUT;
		fds.push_back(pfd);
	}
}

void Server::run()
{
	std::vector<struct pollfd> fds;

	while (!_stop)
	{
		try
		{
			buildPollFds(fds);
			// The timeout only exists so that closing connections whose
			// grace period expired get reaped even when nothing happens.
			if (poll(&fds[0], fds.size(), 1000) <= 0)
			{
				reapClients();
				continue;
			}
			for (size_t i = 0; i < fds.size(); ++i)
			{
				if (fds[i].revents == 0)
					continue;
				if (fds[i].fd == _listenFd)
				{
					if (fds[i].revents & POLLIN)
						acceptClient();
					continue;
				}

				std::map<int, Client *>::iterator it = _clients.find(fds[i].fd);
				if (it == _clients.end())
					continue;
				Client &client = *it->second;

				// A failure while serving one client (including running
				// out of memory) costs that client its connection, never
				// the whole server.
				try
				{
					if (fds[i].revents & POLLIN)
						handleRead(client);
					else if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
						disconnect(client, "Connection reset by peer", false);
					if (!client.isDead() && (fds[i].revents & POLLOUT))
						handleWrite(client);
				}
				catch (const std::exception &)
				{
					// reapClients() detaches it from its channels.
					client.setDead();
				}
			}
			reapClients();
		}
		catch (const std::exception &e)
		{
			std::cerr << "Recovered from error: " << e.what() << std::endl;
		}
	}
	std::cout << "\nServer shutting down" << std::endl;
}

// Exactly one accept() per POLLIN on the listening socket.
void Server::acceptClient()
{
	struct sockaddr_in	addr;
	socklen_t			len = sizeof(addr);
	int					fd;

	fd = accept(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), &len);
	if (fd < 0)
		return;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(fd);
		return;
	}
	try
	{
		Client *client = new Client(fd, inet_ntoa(addr.sin_addr));

		try
		{
			_clients[fd] = client;
		}
		catch (...)
		{
			delete client;
			throw;
		}
	}
	catch (const std::exception &)
	{
		_clients.erase(fd);
		close(fd);
		return;
	}
	std::cout << "[+] fd " << fd << " connected from " << inet_ntoa(addr.sin_addr) << std::endl;
}

// Exactly one recv() per POLLIN. The bytes are appended to the client's
// input buffer and only complete lines are executed: a command can arrive
// in several pieces, or several commands in one piece.
void Server::handleRead(Client &client)
{
	char	buf[4096];
	ssize_t	n;

	n = recv(client.getFd(), buf, sizeof(buf), 0);
	if (n <= 0)
	{
		disconnect(client, "Connection closed", false);
		return;
	}
	if (client.isGone())
		return;

	std::string	&in = client.inBuf();
	size_t		pos;

	in.append(buf, static_cast<size_t>(n));
	while (!client.isGone() && (pos = in.find('\n')) != std::string::npos)
	{
		std::string line = in.substr(0, pos);

		in.erase(0, pos + 1);
		// Lines end in CRLF, but a bare LF (nc without -C) is accepted too.
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (!line.empty())
			execute(client, line);
	}
	if (!client.isGone() && in.size() > MAX_INPUT)
		disconnect(client, "Input line too long", true);
}

// Exactly one send() per POLLOUT. Whatever the kernel did not take stays
// in the buffer for the next POLLOUT.
void Server::handleWrite(Client &client)
{
	std::string	&out = client.outBuf();
	ssize_t		n;

	if (out.empty())
		return;
	n = send(client.getFd(), out.data(), out.size(), 0);
	if (n <= 0)
	{
		disconnect(client, "Connection reset by peer", false);
		return;
	}
	out.erase(0, static_cast<size_t>(n));
}

// Takes a client out of the chat immediately (its channels are told and it
// is removed from all of them), but the object itself is only freed by
// reapClients() at the end of the loop iteration. That way no code ever
// holds a reference to a deleted client.
//
// flush == true : the peer is still there, let it receive what is queued.
// flush == false: the peer is gone, just drop it.
void Server::disconnect(Client &client, const std::string &reason, bool flush)
{
	if (client.isGone())
	{
		if (!flush)
			client.setDead();
		return;
	}
	if (client.isRegistered())
		broadcastToPeers(client, ":" + client.prefix() + " QUIT :" + reason, false);

	std::map<std::string, Channel *>::iterator it = _channels.begin();
	while (it != _channels.end())
	{
		Channel *channel = it->second;

		++it;
		leaveChannel(client, channel);
	}
	if (flush)
	{
		client.queue("ERROR :Closing Link: " + client.getHost() + " (" + reason + ")");
		client.setClosing(std::time(NULL) + CLOSE_GRACE);
	}
	else
		client.setDead();
}

void Server::reapClients()
{
	std::time_t now = std::time(NULL);

	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->hasOverflowed() && !it->second->isGone())
			disconnect(*it->second, "SendQ exceeded", false);
	}

	std::map<int, Client *>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		Client	*client = it->second;
		bool	done = client->isDead();

		if (client->isClosing() && (client->outBuf().empty() || now >= client->getDeadline()))
			done = true;
		if (done)
		{
			// Normally a no-op (disconnect() already did it); guarantees
			// that no channel keeps a pointer to the client being freed.
			std::map<std::string, Channel *>::iterator ch = _channels.begin();
			while (ch != _channels.end())
			{
				Channel *channel = ch->second;

				++ch;
				if (channel->hasMember(client))
					leaveChannel(*client, channel);
			}
			std::cout << "[-] fd " << it->first << " disconnected" << std::endl;
			close(it->first);
			delete client;
			_clients.erase(it++);
		}
		else
			++it;
	}
}

void Server::execute(Client &client, const std::string &line)
{
	Message msg(line);

	if (msg.command.empty())
		return;

	std::map<std::string, Handler>::const_iterator it = _handlers.find(msg.command);
	if (it == _handlers.end())
	{
		if (client.isRegistered())
			reply(client, "421", msg.command + " :Unknown command");
		return;
	}
	// Before registration is complete only the handshake commands work.
	if (!client.isRegistered() && msg.command != "CAP" && msg.command != "PASS"
		&& msg.command != "NICK" && msg.command != "USER" && msg.command != "QUIT"
		&& msg.command != "PING" && msg.command != "PONG")
	{
		reply(client, "451", ":You have not registered");
		return;
	}
	(this->*(it->second))(client, msg);
}

/* ************************************************************************** */
/*   Helpers                                                                  */
/* ************************************************************************** */

// Numeric reply:  :ircserv <code> <nick> <text>
void Server::reply(Client &client, const std::string &code, const std::string &text) const
{
	std::string nick = client.getNick().empty() ? "*" : client.getNick();

	client.queue(std::string(":") + SERVER_NAME + " " + code + " " + nick + " " + text);
}

// Clients on their way out are invisible: their nickname is free again.
Client *Server::findClient(const std::string &nick) const
{
	std::string wanted = toLower(nick);

	for (std::map<int, Client *>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (!it->second->isGone() && toLower(it->second->getNick()) == wanted)
			return it->second;
	}
	return NULL;
}

Channel *Server::findChannel(const std::string &name) const
{
	std::map<std::string, Channel *>::const_iterator it = _channels.find(toLower(name));

	if (it == _channels.end())
		return NULL;
	return it->second;
}

// Removes the client from the channel and destroys the channel if that
// leaves it empty.
void Server::leaveChannel(Client &client, Channel *channel)
{
	channel->removeMember(&client);
	if (channel->isEmpty())
	{
		_channels.erase(toLower(channel->getName()));
		delete channel;
	}
}

// Sends a line once to every client sharing at least one channel with
// `client` (used for NICK and QUIT, which concern the user, not a channel).
void Server::broadcastToPeers(Client &client, const std::string &line, bool includeSelf) const
{
	std::set<Client *> peers;

	for (std::map<std::string, Channel *>::const_iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		if (it->second->hasMember(&client))
			peers.insert(it->second->getMembers().begin(), it->second->getMembers().end());
	}
	peers.erase(&client);
	if (includeSelf)
		peers.insert(&client);
	for (std::set<Client *>::iterator it = peers.begin(); it != peers.end(); ++it)
		(*it)->queue(line);
}
