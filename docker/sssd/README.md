# sssd in a container

An image that runs the System Security Services Daemon on its own, so that
other containers get directory (AD, LDAP, Kerberos) users and logins
without carrying sssd or its configuration themselves. Consumers need only
the sssd client libraries (`pam_sss`, `libnss_sss`) and a shared
`/var/lib/sss/pipes` directory, where sssd's responder sockets live.

Nothing in the image is specific to any consumer, and it ships no
configuration: everything comes from a `/config` mount at start.

## Build

```bash
docker build -t sssd docker/sssd
```

## Configuration

Mount a directory at `/config`. The entrypoint copies its contents into
place with the ownership and modes sssd insists on, so the host copies can
have any mode and the mount can be read-only. Each start replaces what the
previous start staged, so removing a fragment or a certificate from
`/config` takes effect on restart.

| in `/config`     | copied to                            | required |
|------------------|--------------------------------------|----------|
| `sssd.conf`      | `/etc/sssd/sssd.conf`                | yes      |
| `conf.d/*.conf`  | `/etc/sssd/conf.d/`                  | no       |
| `krb5.conf`      | `/etc/krb5.conf`                     | no       |
| `krb5.keytab`    | `/etc/krb5.keytab`                   | no       |
| `ca-certs/*`     | the system CA store (as `.crt`)      | no       |

Two `sssd.conf` settings matter for the container case:

- `pam_trusted_users` defaults to all users. If it is restricted, it must
  include the uid of every process in the consumer containers that calls
  `pam_sss`, since they are not root.
- `cache_credentials = True` in the domain section lets logins succeed
  from cached credentials while the directory is unreachable.

## Volumes

| path                  | purpose                                              |
|-----------------------|------------------------------------------------------|
| `/var/lib/sss/pipes`  | responder sockets; share it with every consumer      |
| `/var/lib/sss/db`     | the identity cache; keep it so restarts start warm   |

The public sockets in `pipes` are world-connectable; `pipes/private` is
root-only and is never needed by a consumer.

## Run

```bash
# --hostname: the name the keytab was issued for
docker run -d --name sssd --restart unless-stopped \
    --hostname host.example.com \
    -v /srv/sssd:/config:ro \
    -v sss-pipes:/var/lib/sss/pipes -v sss-db:/var/lib/sss/db \
    sssd
```

sssd runs in the foreground and logs to stderr, so `docker logs sssd` shows
it. `docker exec sssd sssctl domain-status <domain>` reports the backend
state.

## Consumers

A consumer container needs:

1. The client libraries: `sssd-client` on Red Hat family systems,
   `libnss-sss` and `libpam-sss` on Debian family systems. No daemon.
2. `sss` after `files` for `passwd`, `group` and `shadow` in
   `/etc/nsswitch.conf`.
3. `pam_sss.so` in the PAM service that authenticates.
4. The `sss-pipes` volume mounted at `/var/lib/sss/pipes`.

Check with `getent passwd <directory user>` inside the consumer. It has to
resolve as any uid, root or not. Without the sidecar running, lookups and
`pam_sss` simply fail and the rest of the PAM stack decides.
