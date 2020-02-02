# Network chat
Just a basic network chat program built while learning about sockets in C.

## Installing
You can download the client and server files, or alternatively compile server and client c files (in srccode folder) yourself -- make sure to link commonfunc.c to both.

## Running
Functionality is pretty basic, but run the server on one machine (or one terminal on the same machine), then run clients with the first argument as the IPV4 address of the server in dot format. (Doesn't have IPV6 support yet).

The server receives messages and forwards them on to all clients.

## To do
I need to add some kind of username identification so you can see who messages come from
Encryption when sending?
