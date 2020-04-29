# Network chat
Just a basic network chat program built while learning about sockets in C.

**Mostly surpassed by https://github.com/zachng1/chatroom which implements a basic GUI. However does not have the encryption available in this version yet.**

## Compatibility
Uses Linux sockets for IPC, so probably won't work on Windows. (Considering changing from processes to threading though)

## Installing
Compile server and client c files (in srccode folder) yourself -- make sure to link commonfunc.c to both.

## Running
Functionality is pretty basic. Run the server on one machine (or one terminal on the same machine), then run clients. (Doesn't have IPV6 support yet).

The server receives messages and forwards them on to all clients.

Server usage:
server port

Client usage:
client ipaddress port displayname
