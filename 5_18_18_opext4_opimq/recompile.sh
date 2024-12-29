#!/bin/sh
make -j40
make modules -j40
make modules_install -j40
make install -j40
