#!/bin/bash

if [[ $1 == 'mount' ]]; then
	echo 'Mounting 16GB of tmpfs at ./debugspace.'
	sudo mount tmpfs ./debugspace -v -t tmpfs -o size=16G
	df -h ./debugspace
elif [[ $1 == 'umount' ]]; then
	echo 'Umounting tmpfs at ./debugspace.'
	sudo umount -v ./debugspace
else
	echo 'Use "sh ./this.sh mount" or "sh ./this.sh umount".'
fi