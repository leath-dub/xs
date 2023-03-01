#!/bin/sh

# takes screenshot of first monitor
file=`mktemp`
xs start 0 > $file
ff2png < $file > out.png

# If you had 3 monitors and wanted to show them all
# xs start 0 end 2 > $file
# ff2png < $file > out.png
