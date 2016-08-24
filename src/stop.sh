#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

if [[ ! -e $DIR/webradio.pid ]]; then
    echo Not running
    exit
fi

WEBRADIOPID=`cat $DIR/webradio.pid`

echo Stopping WebRadio [ PID = $WEBRADIOPID ]
kill $WEBRADIOPID
rm $DIR/webradio.pid

GSTREAMERPID=`cat $DIR/gstreamer.pid`

echo Waiting for GStreamer to starve [ PID = $GSTREAMERPID ]
while [[ `ps --no-headers $GSTREAMERPID` ]]; do
    sleep 1
done
rm $DIR/gstreamer.pid

