#!/usr/xpg4/bin/awk -f 
# $Id$
# dshbak  *must have nawk or compatible*
BEGIN { LASTNODE="null" 
	FS="\t"}
{
	if ($1 != LASTNODE) {
		LASTNODE = $1
		FOO = $1
		gsub(/./,"-",FOO)
		printf("-----%s\nNode %s\n-----%s\n", FOO, $1, FOO)
	}
	sub(/^.*:\t/,"")
	print $0
}
	
