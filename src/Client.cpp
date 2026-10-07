#include "Client.hpp"

// A client that stops reading (suspended, or a dead link not yet detected)
// keeps accumulating output. Past this size it is disconnected instead of
// letting it eat the server's memory.
static const size_t MAX_SENDQ = 8 * 1024 * 1024;

Client::Client(int fd, const std::string &host)
	: _fd(fd), _host(host), _password(false), _registered(false),
	  _closing(false), _dead(false), _overflow(false), _deadline(0)
{
}

Client::~Client()
{
}

int					Client::getFd() const { return _fd; }
const std::string	&Client::getHost() const { return _host; }
const std::string	&Client::getNick() const { return _nick; }
const std::string	&Client::getUser() const { return _user; }
const std::string	&Client::getRealName() const { return _real; }

std::string Client::prefix() const
{
	return _nick + "!" + _user + "@" + _host;
}

void Client::setNick(const std::string &nick) { _nick = nick; }

void Client::setUser(const std::string &user, const std::string &real)
{
	_user = user;
	_real = real;
}

bool	Client::hasPassword() const { return _password; }
void	Client::setPassword(bool ok) { _password = ok; }
bool	Client::isRegistered() const { return _registered; }
void	Client::setRegistered() { _registered = true; }

std::string	&Client::inBuf() { return _in; }
std::string	&Client::outBuf() { return _out; }

void Client::queue(const std::string &line)
{
	if (_dead)
		return;
	if (_out.size() + line.size() > MAX_SENDQ)
	{
		_overflow = true;
		return;
	}
	_out += line;
	_out += "\r\n";
}

bool	Client::hasPendingOutput() const { return !_out.empty(); }
bool	Client::hasOverflowed() const { return _overflow; }
bool	Client::isClosing() const { return _closing; }
bool	Client::isDead() const { return _dead; }
bool	Client::isGone() const { return _closing || _dead; }

void Client::setClosing(std::time_t deadline)
{
	_closing = true;
	_deadline = deadline;
}

void Client::setDead()
{
	_dead = true;
	_out.clear();
}

std::time_t Client::getDeadline() const { return _deadline; }
