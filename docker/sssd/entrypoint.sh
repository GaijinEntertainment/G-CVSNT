#!/bin/bash
# Stage the configuration mounted at /config (see README.md) where sssd
# expects it, with the ownership and modes sssd insists on, then run sssd
# in the foreground.
set -eu

# Everything staged by a previous start is replaced, so that files removed
# from /config disappear on restart too (the container layer persists).
install -o root -g root -m 0600 /config/sssd.conf /etc/sssd/sssd.conf
rm -rf /etc/sssd/conf.d && mkdir -p /etc/sssd/conf.d
for f in /config/conf.d/*.conf; do
    [ -f "$f" ] && install -o root -g root -m 0600 "$f" /etc/sssd/conf.d/
done
rm -f /etc/krb5.conf /etc/krb5.keytab
[ -f /config/krb5.conf ]   && install -o root -g root -m 0644 /config/krb5.conf   /etc/krb5.conf
[ -f /config/krb5.keytab ] && install -o root -g root -m 0600 /config/krb5.keytab /etc/krb5.keytab
# update-ca-certificates only picks up *.crt; --fresh drops certificates
# whose source file is gone
staged=/usr/local/share/ca-certificates/config
rm -rf "$staged" && mkdir -p "$staged"
for f in /config/ca-certs/*; do
    [ -f "$f" ] && cp "$f" "$staged/$(basename "${f%.*}").crt"
done
update-ca-certificates --fresh >/dev/null

# The socket directory is a shared volume: make sure sssd owns it and that
# the public sockets can be reached by the (non-root) cvsnt server.
chown root:root /var/lib/sss/pipes
chmod 0755 /var/lib/sss/pipes
mkdir -p /var/lib/sss/pipes/private && chmod 0700 /var/lib/sss/pipes/private

# -i: foreground; logs go to stderr so `docker logs` shows them.
exec /usr/sbin/sssd -i --logger=stderr "$@"
