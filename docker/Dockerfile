# G-CVSNT server images.
#
# Build context is the repository root. One build stage compiles the server
# tree once; each runtime target copies /usr/local/cvsnt out of it.
#
#   docker build -f docker/Dockerfile --target authserver  -t cvsnt-authserver  .
#   docker build -f docker/Dockerfile --target cafs-server -t cvsnt-cafs-server .
#   docker build -f docker/Dockerfile --target cafs-proxy  -t cvsnt-cafs-proxy  .
#   docker build -f docker/Dockerfile --target cvslockd    -t cvsnt-cvslockd    .
#
# docker/README.md explains how to deploy them.

ARG BASE=rockylinux:8

# ---------------------------------------------------------------- build ----
FROM ${BASE} AS build

RUN dnf install -y epel-release dnf-plugins-core \
 && dnf config-manager --set-enabled powertools \
 && dnf install -y gcc-toolset-9-gcc gcc-toolset-9-gcc-c++ \
        autoconf automake m4 make libtool libtool-ltdl-devel \
        libxml2-devel pcre-devel libzstd-devel zlib-devel openssl-devel pam-devel \
 && dnf clean all

COPY cvsnt/cvsnt-2.5.05.3744 /src
WORKDIR /src

RUN scl enable gcc-toolset-9 -- bash -eu -c ' \
      autoreconf -fi && \
      ./configure --prefix=/usr/local/cvsnt \
          --enable-pam --enable-server --enable-pserver --enable-sspi --enable-ext \
          --disable-mysql --disable-postgres --disable-sqlite --enable-64bit \
          --disable-avx512 && \
      export LD_LIBRARY_PATH=/src/ca_blobs_fs/.libs:/src/blake3/.libs && \
      make -j"$(nproc)" && \
      make install && \
      g++ -std=c++11 tools/simplelock.cpp -o /usr/local/cvsnt/bin/simplelock && \
      g++ -std=c++11 tools/unlock.cpp     -o /usr/local/cvsnt/bin/unlock'

# -------------------------------------------------------------- runtime ----
FROM ${BASE} AS runtime

RUN dnf install -y epel-release \
 && dnf install -y libtool-ltdl libxml2 pcre libzstd zlib openssl-libs openssl pam \
 && dnf clean all \
 && groupadd -g 52 cvs && useradd -u 52 -g 52 -d /var/lib/cvs -M cvs

COPY --from=build /usr/local/cvsnt /usr/local/cvsnt

# configuration lives in /etc/cvsnt (mount it); the tree was configured with
# its etc under the prefix, so point that at /etc/cvsnt
RUN mkdir -p /etc/cvsnt \
 && rm -rf /usr/local/cvsnt/etc/cvsnt \
 && ln -s /etc/cvsnt /usr/local/cvsnt/etc/cvsnt

ENV PATH=/usr/local/cvsnt/bin:$PATH

# ----------------------------------------------------------- authserver ----
# pserver (:pserver: on 2401) behind xinetd; authenticates against
# CVSROOT/passwd and, failing that, PAM, which here means the sssd sidecar:
# the server runs as cvs and cannot verify local accounts through pam_unix.
#
# sssd itself is not in this image. sssd-client only provides pam_sss and
# libnss_sss, which talk to a daemon over the sockets in /var/lib/sss/pipes;
# mount that directory from the docker/sssd sidecar to enable directory
# logins. Without the mount the PAM stack ends at pam_unix.
FROM runtime AS authserver

RUN dnf install -y xinetd sssd-client && dnf clean all \
 && mkdir -p /var/log/xinetd /var/lib/sss/pipes \
 && sed -i -E 's/^(passwd|group|shadow):.*/\1: files sss/' /etc/nsswitch.conf

COPY docker/authserver/xinetd-cvsnt /etc/xinetd.d/cvsnt
COPY docker/authserver/pam-cvsnt    /etc/pam.d/cvsnt

EXPOSE 2401
ENTRYPOINT ["/usr/sbin/xinetd", "-dontfork", "-stayalive"]

# ---------------------------------------------------------- cafs-server ----
# content-addressed blob store (2403)
FROM runtime AS cafs-server

COPY docker/cafs-server/entrypoint.sh /entrypoint.sh
RUN chmod 0755 /entrypoint.sh

USER cvs:cvs
EXPOSE 2403
ENTRYPOINT ["/entrypoint.sh"]

# ----------------------------------------------------------- cafs-proxy ----
# caching blob proxy for remote sites (2403)
FROM runtime AS cafs-proxy

COPY docker/cafs-proxy/entrypoint.sh /entrypoint.sh
RUN chmod 0755 /entrypoint.sh \
 && mkdir -p /var/cache/cafs && chown cvs:cvs /var/cache/cafs

USER cvs:cvs
EXPOSE 2403
ENTRYPOINT ["/entrypoint.sh"]

# ------------------------------------------------------------- cvslockd ----
# lock server (2402)
FROM runtime AS cvslockd

USER cvs:cvs
EXPOSE 2402
ENTRYPOINT ["/usr/local/cvsnt/bin/cvslockd", "-n", "-g"]
