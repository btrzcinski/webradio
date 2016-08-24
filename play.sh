#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

if [[ ! -d $DIR/logs ]]; then
    mkdir $DIR/logs
fi

( $DIR/webradio.out $* 2>$DIR/logs/webradio.log & echo $! >$DIR/webradio.pid ) |
    gst-launch-1.0 fdsrc ! aacparse ! faad ! audioconvert ! alsasink >logs/gstreamer.log 2>&1 &
echo $! > $DIR/gstreamer.pid

echo Started

