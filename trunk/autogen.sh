#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="Irssi"

if test ! -f $srcdir/configure.in; then
    echo -n "**Error**: Directory \`$srcdir\' does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
fi

# get versions
version_date=`date +%Y%m%d`

echo "/* automatically created by autogen.sh */" > irssi-version.h.in
echo "#define IRSSI_VERSION \"@VERSION@\"" >> irssi-version.h.in
echo "#define IRSSI_VERSION_DATE \"$version_date\"" >> irssi-version.h.in

# create POTFILES.in
echo "Updating POTFILES.in..."
find src -name '*.c'|grep -v 'src/perl/.*/' > po/POTFILES.in

# create help files
echo "Creating help files..."
perl syntax.pl

files=`echo docs/help/in/*.in|sed -e 's,docs/help/in/Makefile.in ,,' -e 's,docs/help/in/,!,g' -e 's/\.in /.in ?/g'`
cat docs/help/in/Makefile.am.gen|sed "s/@HELPFILES@/$files/g"|sed 's/?/\\?/g'|tr '\!?' '\t\n' > docs/help/in/Makefile.am

files=`echo $files|sed 's/\.in//g'`
cat docs/help/Makefile.am.gen|sed "s/@HELPFILES@/$files/g"|sed 's/?/\\?/g'|tr '\!?' '\t\n' > docs/help/Makefile.am

echo "Checking auto* tools..."

# *********** a bit modified GNOME's macros/autogen.sh **********
DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have \`autoconf' installed to compile $PKG_NAME."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

(grep "^AM_PROG_LIBTOOL" $srcdir/configure.in >/dev/null) && {
  (libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed to compile $PKG_NAME."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.2d.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

grep "^AM_GNU_GETTEXT" $srcdir/configure.in >/dev/null && {
  (gettextize --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`gettext' installed to compile $PKG_NAME."
    echo "Get ftp://alpha.gnu.org/gnu/gettext-0.10.35.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have \`automake' installed to compile $PKG_NAME."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
  NO_AUTOMAKE=yes
}


# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing \`aclocal'.  The version of \`automake'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
}

if test "$DIE" -eq 1; then
  exit 1
fi

if test -z "$*"; then
  echo "**Warning**: I am going to run \`configure' with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo \`$0\'" command line."
  echo
fi

case $CC in
xlc )
  am_opt=--include-deps;;
esac

rm -f aclocal.m4
if grep "^AM_GNU_GETTEXT" configure.in >/dev/null; then
  echo "Creating aclocal.m4 ..."
  test -r aclocal.m4 || touch aclocal.m4
  echo "Running gettextize...  Ignore non-fatal messages."
  echo "no" | gettextize --force --copy
  echo "Making aclocal.m4 writable ..."
  test -r aclocal.m4 && chmod u+w aclocal.m4
fi
if grep "^AM_PROG_LIBTOOL" configure.in >/dev/null; then
  echo "Running libtoolize..."
  libtoolize --force --copy
fi
aclocalinclude="$ACLOCAL_FLAGS -I ."
echo "Running aclocal $aclocalinclude ..."
aclocal $aclocalinclude
if grep "^AM_CONFIG_HEADER" configure.in >/dev/null; then
  echo "Running autoheader..."
  autoheader
fi
echo "Running autoconf ..."
autoconf
echo "Running automake --gnu $am_opt ..."
automake --add-missing --gnu $am_opt

conf_flags="--enable-maintainer-mode --enable-compile-warnings" #--enable-iso-c

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile $PKG_NAME || exit 1
else
  echo Skipping configure process.
fi
