#!/usr/bin/env sh

# Adds timespec_get shim through CPPFLAGS
# Can be sourced to set environment variable `. path/to/script.sh`
# Can be evaluated for command substitution to inline environment variable content `$(path/to/script.sh) make`

[ "$CPPFLAGS" != "" ] && CPPFLAGS="$CPPFLAGS "
export CPPFLAGS=$CPPFLAGS-I$(cd $(dirname $BASH_SOURCE[0]); pwd)
echo "CPPFLAGS=\"$CPPFLAGS\""
