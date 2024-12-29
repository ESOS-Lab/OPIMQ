#!/bin/bash
ctags -R
find ./ -name '*.[chS]' > cscope.file
cscope -i cscope.file
