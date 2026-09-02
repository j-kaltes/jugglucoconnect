Server used by Juggluco to exchange connection data between two instance of Juggluco so that they can communicate via ICE.  
In left middle menu-> Mirror when in Add connection ICE is set or when with AutoQR, Internet is selected. 
Juggluco assumes the server listens at port 6789.

privkey.pem in the servers working directory should contain a private key, fullchain.pem the full chain public key.

The hostname (in double quotes) of the server should be set in Common/src/main/cpp/net/ICE/jugglucoconnect.h
For example:
"servername.org"

## Docker Compose deployment

The server uses TLS on TCP port 6789. Put a readable certificate chain in
`fullchain.pem` and its private key in `privkey.pem`, then run:

```sh
chgrp 65534 fullchain.pem privkey.pem
chmod 640 fullchain.pem privkey.pem
docker compose up -d --build
```

The container runs as UID and GID 65534. Both certificate files must be
readable by that account. Compose refuses to start if either path is missing,
so a typo cannot silently create a directory in place of a certificate file.
The image disables the server's verbose SDP and ICE candidate logging, and
Compose caps the remaining local container logs at two 1 MB files.
The service also limits concurrent handlers, rendezvous state, processes,
memory, and CPU. Certificate and key files are excluded from the image build
context and are available only through the read-only runtime mounts.
The host firewall only needs TCP port 6789:

```sh
ufw allow 6789/tcp
```

TURN traffic does not pass through this service. A TURN server such as coturn
still needs its own listener and relay ports.

## Optional peer generation watch

Newer clients can use `PUT /generation` while ICE is negotiating. Each side
registers a random generation token and waits for the other side's token to
change. This lets a stale negotiation restart promptly when one phone has
already moved to a newer ICE generation.

The endpoint is an additive extension. Existing clients continue to use the
original description, address, done, and failure endpoints. New clients treat
the old server's bad-request response as an unsupported capability and carry
on with the original protocol.

Generation watches are intended for active negotiation only. The server keeps
at most 48 generation labels, expires idle state after 15 minutes, and holds a
watch for at most 45 seconds before the client renews it. Generation tokens,
labels, SDP, and ICE candidates are not written to container logs.
