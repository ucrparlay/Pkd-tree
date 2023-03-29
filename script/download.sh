#!/bin/bash
wget -O ../benchmark/data.zip  https://www.dropbox.com/s/6gqa2todjtnjr2t/data.zip?dl=1
unzip ../benchmark/data.zip -d ../benchmark/
chmod 777 ../benchmark/data/
rm ../benchmark/data.zip
