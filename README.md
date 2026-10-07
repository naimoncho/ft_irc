*This project has been created as part of the 42 curriculum by <login1>[, <login2>].*

# ft_irc

## Description

`ircserv` is an IRC server written in C++98. It accepts many clients at the
same time on a single thread, using non-blocking sockets and one `poll()`
call for every I/O operation (accepting, reading and writing). It does not
implement an IRC client or server-to-server links.

Reference client: **irssi**.

### Features

| Command | Purpose |
|---|---|
| `PASS`, `NICK`, `USER` | Registration (the server password is mandatory) |
| `JOIN`, `PART` | Enter and leave channels |
| `PRIVMSG`, `NOTICE` | Messages to a user or to a channel |
| `TOPIC` | View or change a channel topic |
| `KICK`, `INVITE` | Channel operator commands |
| `MODE` | Channel modes `i`, `t`, `k`, `o`, `l` |
| `QUIT`, `PING`, `PONG` | Connection management |
| `CAP`, `WHO`, `NAMES` | Minimal answers that real clients expect |

The first user to join a channel becomes its operator. A channel is
destroyed when its last member leaves.

### Technical choices

- **One `poll()`, one `recv()` or `send()` per event.** The poll set is
  rebuilt from the client list on every iteration. A socket is only watched
  for `POLLOUT` while its client has pending output.
- **Commands never touch sockets.** A command handler only appends lines to
  the output buffer of the clients concerned; the event loop sends them when
  `poll()` reports the socket writable. A stalled client therefore never
  blocks the others, and receives everything once it reads again.
- **Input is reassembled per client.** Bytes accumulate in a buffer and only
  complete lines are executed, so a command may arrive in several packets or
  several commands in one.
- **`errno` is never read.** A failed `recv()`/`send()` after `poll()`
  reported the socket ready means the connection is lost.
- **Deferred deletion.** A disconnecting client is removed from its channels
  at once but freed only at the end of the loop iteration, so no handler can
  use a deleted client.
- **Failures are contained.** An exception raised while serving one client
  (including `std::bad_alloc`) closes that connection only.
- Nicknames and channel names are case-insensitive.

### Layout

```
includes/            class declarations
src/main.cpp         argument checks, signals
src/Server.cpp       event loop, connection lifecycle, lookups
src/Client.cpp       one connection: identity, buffers, state
src/Channel.cpp      members, operators, invitations, modes
src/Message.cpp      parsing of one IRC line
src/commands/        one file per family of commands
```

## Instructions

```sh
make
./ircserv <port> <password>
```

Connect with irssi:

```sh
irssi -c 127.0.0.1 -p 6667 -w <password> -n <nick>
```

or by hand:

```sh
nc -C 127.0.0.1 6667
PASS <password>
NICK alice
USER alice 0 * :Alice
JOIN #42
PRIVMSG #42 :hello
```

`Ctrl+C` stops the server and frees everything.

## Resources

- [Modern IRC Client Protocol](https://modern.ircdocs.horse/) — message
  format, commands and numeric replies
- [RFC 1459](https://datatracker.ietf.org/doc/html/rfc1459) and
  [RFC 2812](https://datatracker.ietf.org/doc/html/rfc2812)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) —
  sockets, `poll()`
- `man 2 poll`, `man 2 socket`, `man 2 fcntl`, `man 7 tcp`

### Use of AI

An AI assistant (Claude) was used for:

- understanding the subject and comparing it with webserv;
- writing a functional test script, which is not part of the submission.

The generated code was then read, tested with the reference client and
reviewed by the authors.
