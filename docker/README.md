# G-CVSNT server in containers

`Dockerfile` builds the server side of this tree once and produces four
images from it:

| target        | port | runs                                            |
|---------------|------|-------------------------------------------------|
| `authserver`  | 2401 | `cvsnt authserver` behind xinetd (the pserver)  |
| `cvslockd`    | 2402 | the lock server                                 |
| `cafs-server` | 2403 | the content-addressed blob store                |
| `cafs-proxy`  | 2403 | a caching blob proxy for remote sites           |

Directory (AD/LDAP/Kerberos) logins are optional and come from an sssd
container that runs next to the authserver; `sssd/README.md` describes that
image on its own, and "Directory logins" below describes how the authserver
uses it.

The rest of this file is the deployment procedure, from an empty host to a
working repository.

## Requirements

- Docker on an x86_64 host. Nothing else is installed on the host.
- Two TCP ports reachable by the clients: 2401 (pserver) and 2403 (blobs).
  The lock server is only ever reached by the other containers.
- A directory for the repository (`/srv/cvsnt/repo` below). It is written
  by uid 52 (`cvs`) from inside the containers.
- A directory for the server configuration (`/srv/cvsnt/conf` below).
- A shared secret of at least 16 characters (`BLOB_SECRET` below). The
  blob server and the pserver configuration both need it, and every
  client presents it to the blob server.
- Clients talk to the blob server *directly*, at the address the pserver
  hands them (`BlobEncryptedURL0`). That address must be the host name or
  IP the clients can reach, not a container name.

## 1. Build the images

From the repository root:

```bash
docker build -f docker/Dockerfile --target authserver  -t cvsnt-authserver  .
docker build -f docker/Dockerfile --target cvslockd    -t cvsnt-cvslockd    .
docker build -f docker/Dockerfile --target cafs-server -t cvsnt-cafs-server .
docker build -f docker/Dockerfile --target cafs-proxy  -t cvsnt-cafs-proxy  .
```

To keep more than one build deployable, add the cvsnt version or the
commit to the tags (`cvsnt-authserver:3.5.24.7699`) and refer to that tag
in the run commands below.

## 2. Write the server configuration

`/etc/cvsnt` inside the containers is where `cvsnt` and `cvslockd` read
their configuration; the directory is bind-mounted from the host.

```bash
BLOB_SECRET=<otp secret>               # at least 16 characters; used again in step 5
mkdir -p /srv/cvsnt/repo /srv/cvsnt/conf
: > /srv/cvsnt/conf/Plugins            # must exist, may be empty
cat > /srv/cvsnt/conf/PServer <<EOF
# repositories: /cvs is the mount point of /srv/cvsnt/repo in the containers
Repository0=/cvs/main
Repository0Name=/main
Repository0Description=main repository
Repository0Default=1
Repository0Online=1

# lock server, by container name on the docker network created in step 5
LockServer=cvslockd:2402

# The pserver runs as user cvs and cannot switch to the uid of a login it
# authenticated through PAM, so keep every session on cvs. The repository
# is owned by cvs anyway. Logins from CVSROOT/passwd map to cvs through
# their third field and work either way.
RunAsUser=cvs

# What the clients are told to use for blob traffic: must be reachable
# from the client machines. Replace the host name.
BlobEncryptedURL0=cvs.example.com@2403
# shared secret with the blob server
BlobOTP=$BLOB_SECRET

# The pserver hands each client a blob token over this connection. With
# EncryptionLevel=0 that connection is plain text, so either raise the
# level to have clients encrypt it, or keep port 2401 on a trusted network.
EncryptionLevel=0
CompressionLevel=0
EOF
```

`cvsnt/cvsnt-2.5.05.3744/doc/PServer.example` documents every key.

The file holds the blob secret, and the servers read it as uid 52, so keep
it readable by that uid only:

```bash
chown -R root:52 /srv/cvsnt/conf
chmod 750 /srv/cvsnt/conf && chmod 640 /srv/cvsnt/conf/*
```

## 3. Initialise the repository

Run `cvsnt init` from the authserver image, as root, with a throwaway
configuration that has no lock server (the container is not on the
network yet). `-n` keeps `init` from writing into the configuration.

```bash
docker run --rm -v /srv/cvsnt/repo:/cvs --entrypoint sh cvsnt-authserver -c '
    printf "LockServer=none\n" > /etc/cvsnt/PServer
    cvsnt -d /cvs/main init -n
    mkdir -p /cvs/main/blobs       # blob store root; nothing else creates it
    chown -R cvs:cvs /cvs/main'
```

## 4. Create the first login

Logins are checked against `CVSROOT/passwd` first: one line per user,
`login:crypt-hash:system-user`, where the system user is always `cvs`
here. Any hash the system `crypt()` verifies is accepted; `openssl` in the
image produces a SHA-512 one.

```bash
docker run --rm -v /srv/cvsnt/repo:/cvs --entrypoint sh cvsnt-authserver -c '
    echo "alice:$(openssl passwd -6 "her password"):cvs" >> /cvs/main/CVSROOT/passwd
    chown cvs:cvs /cvs/main/CVSROOT/passwd'
```

Logins not listed there fall through to PAM, which only succeeds with the
sssd sidecar (see "Directory logins"). To refuse them outright, set
`SystemAuth=no` in `/srv/cvsnt/repo/main/CVSROOT/config`.

## 5. Start the servers

```bash
docker network create cvsnt

docker run -d --name cvslockd --network cvsnt --restart unless-stopped \
    --ulimit nofile=65536:65536 \
    -v /srv/cvsnt/conf:/etc/cvsnt:ro \
    cvsnt-cvslockd

docker run -d --name cafs --network cvsnt --restart unless-stopped -t \
    --ulimit nofile=65536:65536 \
    -v /srv/cvsnt/repo:/cvs \
    -e DIR_FOR_ROOTS=/cvs -e ALLOW_TRUST=on -e SECRET="$BLOB_SECRET" \
    -p 2403:2403 \
    cvsnt-cafs-server

docker run -d --name authserver --network cvsnt --restart unless-stopped \
    --ulimit nofile=65536:65536 \
    -v /srv/cvsnt/repo:/cvs -v /srv/cvsnt/conf:/etc/cvsnt:ro \
    -p 2401:2401 \
    cvsnt-authserver
```

`--ulimit nofile`: every client session holds sockets and repository files
open in all three servers, and Docker's default limit of 1024 is reached
with a few hundred concurrent clients. `-t` on the blob server: it logs
with plain `printf`, which stays buffered without a terminal and leaves
`docker logs` empty. The blob server requires the secret from every client
(`ENCRYPTION=mandatory_encryption`, the default); `ENCRYPTION=encryption`
waives it for clients on private-network addresses, which on a LAN means
everyone. The entrypoint scripts
(`cafs-server/entrypoint.sh`, `cafs-proxy/entrypoint.sh`) list every
variable with its default; only `SECRET`, and `MASTER_URL` for the proxy,
have none. Both daemons take the secret on their command line, so any
local account on the host can read it from the process list: the host
itself has to be trusted.

## 6. Check

From a client machine:

```bash
cvsnt -d :pserver:alice@cvs.example.com:2401:/main login
cvsnt -d :pserver:alice@cvs.example.com:2401:/main co -l .     # empty checkout
```

Server-side logs: `docker logs` for cvslockd and cafs;
`/var/log/xinetd/xinetd.log` inside the authserver container. A blob push
that fails while text commits work means the clients cannot reach
`BlobEncryptedURL0`.

Backups: `/srv/cvsnt/repo` is the whole state. `/srv/cvsnt/conf` holds the
configuration, and `/srv/cvsnt/sssd` the directory-login configuration
when the sssd container is used.

## The same stack with Compose

An equivalent of step 5, as an example to adapt (`BLOB_SECRET` from the
environment or a `.env` file next to it):

```yaml
name: cvsnt
x-server: &server
  restart: unless-stopped
  ulimits:
    nofile: {soft: 65536, hard: 65536}
services:
  cvslockd:
    <<: *server
    image: cvsnt-cvslockd
    volumes: [/srv/cvsnt/conf:/etc/cvsnt:ro]
  cafs:
    <<: *server
    image: cvsnt-cafs-server
    tty: true
    environment: {DIR_FOR_ROOTS: /cvs, ALLOW_TRUST: "on", SECRET: "${BLOB_SECRET:?}"}
    volumes: [/srv/cvsnt/repo:/cvs]
    ports: ["2403:2403"]
  authserver:
    <<: *server
    image: cvsnt-authserver
    depends_on: [cvslockd, cafs]
    volumes: [/srv/cvsnt/repo:/cvs, /srv/cvsnt/conf:/etc/cvsnt:ro]
    ports: ["2401:2401"]
```

## Directory logins

The authserver image contains the sssd client libraries and `sss` in its
`nsswitch.conf`, nothing more: logins that are not in `CVSROOT/passwd` go
through `pam_sss`, which talks to an sssd daemon over the sockets in
`/var/lib/sss/pipes`. Run that daemon as described in `sssd/README.md`,
with its configuration directory at `/srv/cvsnt/sssd` in this layout, and
give the authserver the same socket volume:

```bash
# the authserver from step 5, plus the socket volume of the sssd container
docker rm -f authserver
docker run -d --name authserver --network cvsnt --restart unless-stopped \
    --ulimit nofile=65536:65536 \
    -v /srv/cvsnt/repo:/cvs -v /srv/cvsnt/conf:/etc/cvsnt:ro \
    -v sss-pipes:/var/lib/sss/pipes \
    -p 2401:2401 \
    cvsnt-authserver
```

Then `docker exec authserver getent passwd <directory user>` must resolve,
and that user can `cvsnt login` with the directory password.

Three things on the cvsnt side:

- Keep `RunAsUser=cvs` from step 2. Without it a directory login fails
  after authentication, when the server tries to switch to the user's own
  gid, which it cannot do as `cvs`.
- The pserver calls `pam_sss` as uid 52. If `pam_trusted_users` in
  `sssd.conf` is restricted, that uid must be in it.
- Every account the directory authenticates can then log in, and every
  session runs as `cvs` with full access to the repository. Restrict who
  may log in with sssd's access control in `sssd.conf`
  (`access_provider`, for example `simple_allow_groups`).
- Local accounts inside the authserver container cannot log in through
  PAM: `pam_unix` needs root to verify another user's password. The two
  working paths are `CVSROOT/passwd` and sssd.

## A blob proxy at a remote site

```bash
# the proxy runs as uid 52 and creates its cache below this directory
mkdir -p /srv/cafs-cache && chown 52:52 /srv/cafs-cache

docker run -d --name cafs-proxy --restart unless-stopped -t \
    --ulimit nofile=65536:65536 \
    -v /srv/cafs-cache:/var/cache/cafs \
    -e MASTER_URL=cvs.example.com -e BLOB_OTP="$BLOB_SECRET" \
    -p 2403:2403 \
    cvsnt-cafs-proxy
```

`MASTER_URL` is the blob server's host name only; the proxy always
connects to it on port 2403.

Clients at that site then use `--blob_url <proxy host>@2403`, or
`BlobEncryptedURL0` in a pserver configuration that points at the proxy.
