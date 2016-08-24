#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

G_MESSAGES_DEBUG=all $DIR/play.sh $*

