#!/bin/bash

cd web

for file in *
do
    if [[ -f $file ]]; then
        gzip -k -c $file > ../web-gz/$file
    fi
done

echo "Web files are gzipped"
