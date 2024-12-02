# Internet Protocols project

The project consists of two parts, a client and a server.

## Client

Connects to a given remote host, specified by the index of its entry in `src/destinations.c`.

By default it connects over IPv6, but it can be configured to connect over IPv4 by having the environment variable `USE_IPV4=1`.

## Server

Runs on port `22034` (port is chosen as `22GSE` where G is the group, S is the semigroup, and E is the team number).

Assigned command is `E` (same meaning as above), namely `4`.

The server is configured to accept commands from a special client that sends messages in the form `xy#`, where x and y are digits. The client has a preconfigured list of commands ranging from `01#` to `24#`.

The server responds to known commands (aka assigned command) with the response it gets by calling the client with the given number. This returns a HTTP response with headers and usually HTML content.

The server responds to unknown commands (any other than the assigned command) by sending "Command not implemented".

This is just an artificial limitation which can be removed by having the environment variable `ALL_COMMANDS=1` set.

Both the server and the client normally show little output, but can be made more verbose using the environment variable `DEBUG=1`.

NOTE: due to some ISP issues, HTTP requests over IPv6 would not be sent from the (physical) server the program runs on. To still make use of the IPv6 capabilities of the program, the server admins spun up a local HTTP server that runs on [::1]:80.

To use this functionality instead of the normal one, set the environment variable `LOCALHOST=1`.