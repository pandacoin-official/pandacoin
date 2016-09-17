#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/pandacoin.ico

convert ../../src/qt/res/icons/pandacoin-16.png ../../src/qt/res/icons/pandacoin-32.png ../../src/qt/res/icons/pandacoin-48.png ${ICON_DST}
