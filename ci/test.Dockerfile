ARG BASE=cvsnt-build
FROM ${BASE}
RUN dnf install -y python39 diffutils findutils && dnf clean all
