#!/bin/bash
find ./ -name '*.[cCsShH]' > file_list
cscope -i file_list
