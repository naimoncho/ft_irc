#ifndef SERVER_HPP
# define SERVER_HPP

# include <string>
# include <vector>
# include <map>
# include <csignal>
# include <poll.h>

# include "Client.hpp"
# include "Channel.hpp"
# include "Message.hpp"

# define SERVER_NAME "ircserv"

class Server
{
	public:
		Server(int port, const std::string &password);
		~Server();

		// Runs the event loop until stop() is called (SIGINT / SIGTERM).
		void		run();
		static void	stop();

	private:
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		typedef void (Server::*Handler)(Client &, const Message &);

		// ---- network (Server.cpp) ----
		void	setupListener(int port);
		void	buildPollFds(std::vector<struct pollfd> &fds) const;
		void	acceptClient();
		void	handleRead(Client &client);
		void	handleWrite(Client &client);
		void	disconnect(Client &client, const std::string &reason, bool flush);
		void	reapClients();
		void	execute(Client &client, const std::string &line);

		// ---- helpers (Server.cpp) ----
		void	reply(Client &client, const std::string &code, const std::string &text) const;
		Client	*findClient(const std::string &nick) const;
		Channel	*findChannel(const std::string &name) const;
		void	leaveChannel(Client &client, Channel *channel);
		void	broadcastToPeers(Client &client, const std::string &line, bool includeSelf) const;

		// ---- commands/Registration.cpp ----
		void	cmdCap(Client &client, const Message &msg);
		void	cmdPass(Client &client, const Message &msg);
		void	cmdNick(Client &client, const Message &msg);
		void	cmdUser(Client &client, const Message &msg);
		void	cmdPing(Client &client, const Message &msg);
		void	cmdPong(Client &client, const Message &msg);
		void	cmdQuit(Client &client, const Message &msg);
		void	tryRegister(Client &client);

		// ---- commands/Channels.cpp ----
		void	cmdJoin(Client &client, const Message &msg);
		void	cmdPart(Client &client, const Message &msg);
		void	cmdTopic(Client &client, const Message &msg);
		void	cmdNames(Client &client, const Message &msg);
		void	cmdWho(Client &client, const Message &msg);
		void	joinChannel(Client &client, const std::string &name, const std::string &key);
		void	sendNames(Client &client, const Channel &channel) const;

		// ---- commands/Messaging.cpp ----
		void	cmdPrivmsg(Client &client, const Message &msg);
		void	cmdNotice(Client &client, const Message &msg);
		void	relay(Client &client, const Message &msg, bool notice);

		// ---- commands/Operators.cpp ----
		void	cmdKick(Client &client, const Message &msg);
		void	cmdInvite(Client &client, const Message &msg);

		// ---- commands/Mode.cpp ----
		void	cmdMode(Client &client, const Message &msg);
		void	applyChannelModes(Client &client, Channel &channel, const Message &msg);

		int								_listenFd;
		std::string						_password;
		std::map<int, Client *>			_clients;	// fd -> client (owned)
		std::map<std::string, Channel *>	_channels;	// lowercase name -> channel (owned)
		std::map<std::string, Handler>	_handlers;

		static volatile sig_atomic_t	_stop;
};

#endif
