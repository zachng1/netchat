# Network chat
Just a basic network chat program built while learning about sockets in C.

# Installing
You can download the client and server .exe files, or alternatively compile server and client c files (in srccode folder) yourself -- make sure to link commonfunc.c to both.

# Running
Functionality is pretty basic, but run the server on one machine (or one terminal on the same machine), then run client with the first argument as the IPV4 address of the server in dot format. (Doesn't have IPV6 support yet).

You can send messages from client to server; and from server to client. Currently no clean way to close, as an interrupt on either side is not recognised by the other. 