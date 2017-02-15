# pcat
A P2P stream broadcaster

# Usage

````
pcat <port>  # Listen on port <port>
````
````
pcat <port> <ip:port> # Join an existing P2P network via the machine "ip:port"
````

pcat maintains a all-to-all network with hot reconnection

Any character fed through stdin is broadcasted to all peers.

Characters received from other peers are output to stdout.

If a line of the form "+ip:port" is found on stdin, it is interpreted as a request to connect to the given peer at ip:port
