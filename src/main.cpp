#include "Server.hpp"

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <exception>

static void onSignal(int)
{
	Server::stop();
}

// Accepts only decimal digits and the range 1-65535.
static bool parsePort(const std::string &arg, int &port)
{
	if (arg.empty() || arg.size() > 5)
		return false;
	for (size_t i = 0; i < arg.size(); ++i)
	{
		if (arg[i] < '0' || arg[i] > '9')
			return false;
	}
	port = std::atoi(arg.c_str());
	return port >= 1 && port <= 65535;
}

int main(int argc, char **argv)
{
	int port;

	if (argc != 3)
	{
		std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
		return 1;
	}
	if (!parsePort(argv[1], port))
	{
		std::cerr << "Error: port must be a number between 1 and 65535" << std::endl;
		return 1;
	}
	std::string password(argv[2]);
	if (password.empty() || password.find(' ') != std::string::npos)
	{
		std::cerr << "Error: password must not be empty or contain spaces" << std::endl;
		return 1;
	}

	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);
	// Writing to a socket whose peer vanished must not kill the process.
	std::signal(SIGPIPE, SIG_IGN);

	try
	{
		Server server(port, password);

		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
