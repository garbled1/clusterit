#!/bin/sh
# $Id$
args=`getopt dfrPRWw: $*`
if test $? != 0
then
	echo 'Usage: prm [-dfrPRW] [-w node1,...,nodeN] file ...'
	exit 2
fi
set -- $args
for i
do
	case "$i"
	in
		-d|-f|-r|-P|-R|-W)
			flag=`echo $flag $i`; shift;;
		-w)
			warg=$2; shift; shift;;
		--)
			shift; break;;
	esac
done
if [ -z $warg ]; then
	dsh rm $flag $*
else
	dsh -w $warg rm $flag $*
fi
