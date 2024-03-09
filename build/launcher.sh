#!/bin/bash

# match -c and -o
path_compile=
path_output=
for ((arg=1; arg<=$#; arg++)); do
    next=$((arg+1))
	if [ "${!arg}" == "-c" ]; then
		path_compile="${!next}"
	elif [ "${!arg}" == "-o" ]; then
		path_output="${!next}"
	fi
done

if [ -z "$path_compile" ]; then
	echo "$1 -o $path_output"
else
	path_rel=`echo ${path_compile} | awk -F "/" '{ print $(NF-1) "/" $NF }'`
	echo "$1 -c ${path_rel}"
fi

exec "$@"
