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

On the wire, a message is formed of:
1. A header of fixed length,
1. A body of variable length, limited though to 4096 bytes for security.

Here's the header format:
1. Message code – 1 byte, used to identify the message type,
1. Message size – 8 bytes, the size of the entire message excluding the header.

Before describing the messages, it is helpful to first describe how strings are sent on the wire. A string in wire format is:
1. Its length – 8 bytes,
1. The string's data – as many as the length specifies.

There is no limit imposed on the length of a string as the whole body is limited anyway.

Going further, there are 4 message kinds:
1. `ClientMessage` – a private/public message from a client,
1. `ServerMessage` – any message sent from the server to the client,
1. `ClientRegistration` – messages from the client with the user name it wishes to identify itself; sent when the client connects to the server,
1. `Disconnect` – sent by the client when it leaves the chat.

The `ClientMessage` body has the format:
1. A boolean (1 byte) specifying whether the message is private or public,
1. If the message is private, a string (as described above) with the desired user name,
1. A string with the message content.

The `ServerMessage` body has the format:
1. A string with the message content.

The `ClientRegistration` body has the format:
1. A string with the user name desired by the client.

`Disconnect` has no body.

Note that the wire protocol does not concern itself with input validation. This is the server's responsibility. Also note that it does not carry the user name of the message sender – the server can determine it.

## The server

## The client