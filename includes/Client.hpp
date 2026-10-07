#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>
# include <ctime>

// State of one TCP connection.
//
// Commands never write to the socket: they call queue(), which appends to
// the output buffer. The server's poll() loop is the only place that calls
// recv() and send().
class Client
{
	public:
		Client(int fd, const std::string &host);
		~Client();

		int					getFd() const;
		const std::string	&getHost() const;
		const std::string	&getNick() const;
		const std::string	&getUser() const;
		const std::string	&getRealName() const;
		// nick!user@host, the source put in front of relayed messages
		std::string			prefix() const;

		void				setNick(const std::string &nick);
		void				setUser(const std::string &user, const std::string &real);

		bool				hasPassword() const;
		void				setPassword(bool ok);
		bool				isRegistered() const;
		void				setRegistered();

		std::string			&inBuf();
		std::string			&outBuf();
		// Appends one protocol line (CRLF is added here).
		void				queue(const std::string &line);
		bool				hasPendingOutput() const;
		bool				hasOverflowed() const;

		// closing: flush what is queued, then drop the connection.
		// dead:    drop the connection now, nothing more can be sent.
		bool				isClosing() const;
		bool				isDead() const;
		bool				isGone() const;
		void				setClosing(std::time_t deadline);
		void				setDead();
		std::time_t			getDeadline() const;

	private:
		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

		int			_fd;
		std::string	_host;
		std::string	_nick;
		std::string	_user;
		std::string	_real;
		std::string	_in;
		std::string	_out;
		bool		_password;
		bool		_registered;
		bool		_closing;
		bool		_dead;
		bool		_overflow;
		std::time_t	_deadline;
};

#endif
