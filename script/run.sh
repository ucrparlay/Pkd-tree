#!/bin/bash

# ./run_inba_ratio.sh
# ./run_highDim.sh

# PARLAY_NUM_THREADS=192 numactl -i all ../build/test -p /data3/zmen002/kdtree/geometry/OpenStreetMap.in -d 2 -r 1 -t 0 -i 0 -q 1 >"test.out"
# PARLAY_NUM_THREADS=192 numactl -i all ../build/cgal -p /data3/zmen002/kdtree/geometry/OpenStreetMap.in -d 2 -r 1 -t 0 -i 0 -q 1 >"cgal.out"
#
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/africa-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/antarctica-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/asia-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/australia-oceania-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/central-america-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/europe-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/north-america-latest.osm.pbf
wget -P /data/zmen002/kdtree/real_world/osm/ https://download.geofabrik.de/south-america-latest.osm.pbf
