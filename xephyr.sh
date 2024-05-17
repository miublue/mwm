#!/bin/bash

Xephyr -screen 1280x700 +xinerama :80 &
sleep 0.1

export DISPLAY=:80

while sleep 1; do ./mwm; done
