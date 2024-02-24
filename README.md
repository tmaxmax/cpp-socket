# termchat

The group chat of your family! Runs on localhost through TCP.

[See demo.](https://youtu.be/MmwF57dfHaE)

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

The server handles the following:
- accepting new clients and handling client messages
- rendering the UI for each client (i.e. building the text the client sees on-screen, kind of how a server builds HTML)
- dispatching messages to the right clients

The philosophy behind is similar to the one behind a classic server-driven website: dumb client, all the state is managed on the server (the thinking HTMX wants to revive). The actions a client can take are determined on the server – this is why we don't need to define distinct wire formats for each message type (user name selection, private message, public message). The server interprets each message according to the state the client is in:
- when the client hasn't yet chosen its user name they can't send messages to other clients – if they send a payload which would be a valid message it is still interpreted as if it was a user name
- conversely, after the user name is chosen, all payloads are interpreted as either private or public messages – the user name can't be set anymore.

From a technical standpoint, the server runs on a single thread and uses `poll` calls to determine which clients have sent payloads. An in-memory registry is used to track the state of each client. Errors are also closely watched – if communication with a client fails, it is removed from the registry and a message is broadcasted to the other clients, announcing that someone was abruptly disconnected.

Please watch the demo to see how the interface looks like.

## The client

The client is very basic. Given that the server basically generates the UI, all that the client has to do is:
- read the user input and send it to the server
- receive data from the server and print it on the screen

These two tasks are executed in their own threads. If the server is closed, the client is prompted to quit. If the client exists (i.e. closes stdin), a disconnect message is sent to the server so that other clients are notified.
