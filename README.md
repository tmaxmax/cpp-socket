# termchat

The group chat of your family! Runs on localhost through TCP.

## General design

Multiple clients connect to a server. Here is the flow of a session:

1. The connection between the client and the server is established
1. The server asks the client to provide a user name for itself
    - The client sends a user name
    - The server validates its format and checks that it is unique
    - If the user name is valid, the flow continues
    - Otherwise the client is asked again for a user name
1. The client can now communicate with other clients
    - Private messages to a specific client can be sent
    - Public messages to all clients can be sent
1. The client can also send a special message to signal that it was disconnected

## Wire protocol

The client and the server communicate with each other through plain strings. A string is serialized as follows:
1. Its length – 8 bytes, limited to 4096;
1. The string's data – as many as the length specifies.

The protocol does not concern itself with parsing these strings – see next chapter.

## The server

TODO

## The client

TODO