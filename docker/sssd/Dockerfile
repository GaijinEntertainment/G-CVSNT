# sssd sidecar for the cvsnt authserver.
#
# Runs the System Security Services Daemon on its own, so the directory
# configuration (sssd.conf, keytab, krb5.conf, CA certificates) lives here
# and the authserver only ever sees the responder sockets. Share
# /var/lib/sss/pipes between the two containers; the authserver's pam_sss
# and libnss_sss connect through them.
#
# Nothing in this image is specific to cvsnt, and it carries no
# configuration: everything is mounted under /config at run time, see
# README.md.
#
#   docker build -t sssd docker/sssd

ARG BASE=debian:bookworm-slim
FROM ${BASE}

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        sssd sssd-tools krb5-user ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && mkdir -p /config /var/lib/sss/pipes/private /var/lib/sss/db /var/log/sssd

COPY entrypoint.sh /entrypoint.sh
RUN chmod 0755 /entrypoint.sh

# pipes: shared with the authserver; db: the identity cache, keep it on a
# volume so it survives restarts.
VOLUME ["/var/lib/sss/pipes", "/var/lib/sss/db"]

ENTRYPOINT ["/entrypoint.sh"]
