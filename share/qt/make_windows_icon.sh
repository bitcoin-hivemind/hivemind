#!/usr/bin/env bash
# create multiresolution windows icon
ICON_SRC=../../src/qt/res/icons/hivemind.png
ICON_DST=../../src/qt/res/icons/hivemind.ico
convert ${ICON_SRC} -resize 16x16 hivemind-16.png
convert ${ICON_SRC} -resize 32x32 hivemind-32.png
convert ${ICON_SRC} -resize 48x48 hivemind-48.png
convert hivemind-16.png hivemind-32.png hivemind-48.png ${ICON_DST}

