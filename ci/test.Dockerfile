# The build stage of docker/Dockerfile plus what the suites and the smoke
# scenario need: python for regress.py/testcvs.py, diffutils/findutils for
# the tree compare. The pipeline never tags the build stage, so it is built
# first with --target build and passed in as BASE.
ARG BASE=cvsnt-build
FROM ${BASE}
RUN dnf install -y python39 diffutils findutils && dnf clean all
