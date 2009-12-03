#!/bin/sh

# This script walks through the master (stdin) mplayer.conf file, and
# prints (stdout) only those formats and fourccs which are missing
# in mplayerxp.conf file given as parameter ($1).
#
# Usage: codec_diff.sh mode codec.conf <mplayer.conf >missing.conf
# mode = --fourcc or --codec
# Example: codec_diff.sh --fourcc codec.conf <mplayer.conf >missing.conf
# Example: codec_diff.sh --codec codec.conf <mplayer.conf >missing.conf

curr=''

case $1 in
--help)
echo "Usage: $0 mode codec.conf <mplayer.conf >missing.conf"
echo "Mode:"
echo "--fourcc    build diff by fourcc and format"
echo "--codec     build diff by videocodec and audiocodec"
;;
--fourcc)
while read -r line; do
	outs=''
	if echo "$line" | grep '^format' > /dev/null 2>&1; then
		outs="$line"
		curr=`echo "$line" | cut -d ' ' -f 2`
		if grep --mmap "format $curr*" $2 > /dev/null 2>&1; then
			outs=''
		fi
	fi
	if echo "$line" | grep '^fourcc' > /dev/null 2>&1; then
		outs="$line"
		curr=`echo "$line" | cut -d ' ' -f 2`
		if grep --mmap "fourcc $curr*" $2 > /dev/null 2>&1; then
			outs=''
		fi
	fi

	if [ -n "$outs" ]; then
		echo "$outs"
	fi
done
;;
--codec)
while read -r line; do
	outs=''
	if echo "$line" | grep '^videcodec' > /dev/null 2>&1; then
		outs="$line"
		curr=`echo "$line" | cut -d ' ' -f 2`
		if grep --mmap "videcodec $curr*" $2 > /dev/null 2>&1; then
			outs=''
		fi
	fi
	if echo "$line" | grep '^audiocodec' > /dev/null 2>&1; then
		outs="$line"
		curr=`echo "$line" | cut -d ' ' -f 2`
		if grep --mmap "audiocodec $curr*" $2 > /dev/null 2>&1; then
			outs=''
		fi
	fi

	if [ -n "$outs" ]; then
		echo "$outs"
	fi
done
;;
*)
;;
esac
