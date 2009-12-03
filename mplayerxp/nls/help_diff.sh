#!/bin/sh

# This script walks through the master (stdin) help/message file, and
# prints (stdout) only those messages which are missing from the help
# file given as parameter ($1).
#
# Example: help_diff.sh nls/help_mp-ru.h UTF-8

MASTER=nls/help_mp-en.h
TARGET=help_mp.h

TRANSLATION=$1
CHARSET=$2

missing_messages(){
curr=""

while read -r line; do
	if echo "$line" | grep -q '^#define' ; then
		curr=`printf "%s\n" "$line" | cut -d ' ' -f 2`
		if grep -q "^#define $curr[	 ]" "$TRANSLATION" ; then
			curr=""
		fi
	else
		if [ -z "$line" ]; then
			curr=""
		fi
	fi

	if [ -n "$curr" ]; then
		printf "%s\n" "$line"
	fi
done
}

cat <<EOF > "$TARGET"
/* WARNING! This is a generated file, do NOT edit.
 * See the help/ subdirectory for the editable files. */

#ifndef MPLAYER_HELP_MP_H
#define MPLAYER_HELP_MP_H

EOF

cat "$TRANSLATION" >> "$TARGET"

cat <<EOF >> "$TARGET"

/* untranslated messages from the English master file */

EOF

if test "$MASTER" != "$TRANSLATION" ; then
    missing_messages < "$MASTER" >> "$TARGET"
fi

cat <<EOF >> "$TARGET"

#endif /* MPLAYER_HELP_MP_H */
EOF
