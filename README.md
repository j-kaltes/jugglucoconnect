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
The host firewall only needs TCP port 6789:

```sh
ufw allow 6789/tcp
```

TURN traffic does not pass through this service. A TURN server such as coturn
still needs its own listener and relay ports.
