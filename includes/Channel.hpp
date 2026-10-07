#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>
# include <set>
# include <ctime>

class Client;

// A channel does not own its clients: it only keeps pointers to objects
// owned by the Server. The Server removes a client from every channel
// before deleting it, so these pointers never dangle.
class Channel
{
	public:
		explicit Channel(const std::string &name);
		~Channel();

		const std::string			&getName() const;
		std::time_t					getCreationTime() const;

		const std::string			&getTopic() const;
		void						setTopic(const std::string &topic);

		// ---- members ----
		const std::set<Client *>	&getMembers() const;
		bool						hasMember(Client *client) const;
		bool						isOperator(Client *client) const;
		bool						isEmpty() const;
		void						addMember(Client *client);
		void						removeMember(Client *client);
		void						setOperator(Client *client, bool on);

		// ---- modes ----
		bool						isInviteOnly() const;
		void						setInviteOnly(bool on);
		bool						isTopicRestricted() const;
		void						setTopicRestricted(bool on);
		const std::string			&getKey() const;
		void						setKey(const std::string &key);
		size_t						getLimit() const;
		void						setLimit(size_t limit);
		bool						isFull() const;

		// ---- invitations (consumed on JOIN) ----
		bool						isInvited(Client *client) const;
		void						addInvite(Client *client);

		// Queues a line for every member, optionally skipping one.
		void						broadcast(const std::string &line, Client *except = NULL) const;
		// "@op1 user2 user3" for RPL_NAMREPLY
		std::string					namesList() const;
		// "+itkl" plus parameters for RPL_CHANNELMODEIS
		std::string					modeString(bool showKey) const;

	private:
		Channel();
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);

		std::string			_name;
		std::string			_topic;
		std::string			_key;
		size_t				_limit;
		bool				_inviteOnly;
		bool				_topicRestricted;
		std::time_t			_created;
		std::set<Client *>	_members;
		std::set<Client *>	_operators;
		std::set<Client *>	_invited;
};

#endif
