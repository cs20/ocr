#!/bin/bash

if [ $# -eq 0 ]
then
echo "Usage: ./highbw.sh <output from code> <debug version of binary>"
exit 0
fi

LIST=`awk -f highbw.awk $1|sort -rnk3 -rnk4 -rnk5 -nk6|cut -d\  -f1|sed 's/0x//g'`
if [ $# -eq 2 ]
then
for i in $LIST
do
nm $2|grep $i
done
else
echo $LIST
fi
