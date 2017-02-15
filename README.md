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

