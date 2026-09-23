#!/bin/bash
WORKING_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
BUILDDIR="$WORKING_DIR/rpmbuild"
PATH_TO_SRC="$WORKING_DIR/.."
VERSION="$(cat $PATH_TO_SRC/version_no.h | grep -E 'CVSNT_PRODUCT_MAJOR|CVSNT_PRODUCT_MINOR|CVSNT_PRODUCT_PATCHLEVEL\s' | sed -e 's/[^0-9]*//' | tr '\n' '.')$(cat $PATH_TO_SRC/build.h | sed -e 's/[^0-9]*//' | tr -d '\n')"
ZLIB_URL="https://github.com/cloudflare/zlib/archive/refs/tags/v1.2.8.tar.gz"
ZLIB_SHA256="e380bd1bdb6447508beaa50efc653fe45f4edc1dafe11a251ae093e0ee97db9a"
cd ../../
cp -rp cvsnt-2.5.05.3744 "cvsnt-${VERSION}"
tar -czf "$WORKING_DIR/cvsnt-${VERSION}.tar.gz" "cvsnt-${VERSION}"
rm -rf "cvsnt-${VERSION}"
cd "$WORKING_DIR"
if [ ! -d $BUILDDIR ]; then
  mkdir -p $BUILDDIR/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi
mv "cvsnt-${VERSION}.tar.gz" "$BUILDDIR/SOURCES"
curl -sSL --fail --connect-timeout 30 --max-time 600 "$ZLIB_URL" -o "$BUILDDIR/SOURCES/zlib-1.2.8.tar.gz" || exit 1
echo "$ZLIB_SHA256  $BUILDDIR/SOURCES/zlib-1.2.8.tar.gz" | sha256sum -c - || exit 1
sed "s/@VERSION@/$VERSION/" <cvsnt.spec.in >"$BUILDDIR/SPECS/cvsnt.spec"
echo "***************** BEGIN RPMBUILD"
echo "`which rpmbuild` -bs $BUILDDIR/SPECS/cvsnt.spec"
CVSNT_VERSION="$VERSION" rpmbuild --define "_topdir $BUILDDIR" -bs "$BUILDDIR/SPECS/cvsnt.spec"

