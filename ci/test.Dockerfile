ARG BASE=cvsnt-runtime
FROM ${BASE}
RUN dnf install -y python39 diffutils findutils && dnf clean all \
 && usermod -d /work cvs
COPY cvsnt/cvsnt-2.5.05.3744/testcvs /src/testcvs
