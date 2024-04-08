import osmium as osm
import pandas as pd
import os

# https://github.com/osmcode/pyosmium/blob/master/examples/amenity_list.py
# https://docs.osmcode.org/pyosmium/latest/


class OSMHandler(osm.SimpleHandler):
    def __init__(self):
        osm.SimpleHandler.__init__(self)
        self.osm_data = []

    def tag_inventory(self, elem, elem_type):
        # for tag in elem.tags:
        self.osm_data.append(
            [
                elem.id,
                pd.Timestamp(elem.timestamp),
                elem.location.lon,
                elem.location.lat,
            ]
        )

    def node(self, n):
        self.tag_inventory(n, "node")


osm_path = "/data/zmen002/kdtree/real_world/osm/src/"
wash_path = "/data/zmen002/kdtree/real_world/osm/wash/"
file_names = os.listdir(osm_path)
# file_names = ["antarctica-latest.osm.pbf"]
# print(file_names, flush=True)
df_osm = pd.DataFrame(
    {
        "lon": [],
        "lat": [],
    }
)

for file in file_names:
    osmhandler = OSMHandler()
    file_path = osm_path + file
    osmhandler.apply_file(file_path)
    data_colnames = [
        "id",
        "ts",
        "lon",
        "lat",
    ]
    df = pd.DataFrame(osmhandler.osm_data, columns=data_colnames)
    print(file, df.index.size, flush=True)
    df.to_csv(wash_path + file + ".csv", index=False)
# for file in file_names:
#     osmhandler = OSMHandler()
#     osmhandler.apply_file(file)
#     data_colnames = [
#         "id",
#         "ts",
#         "lon",
#         "lat",
#     ]
#     df = pd.DataFrame(osmhandler.osm_data, columns=data_colnames)
#     df = df.drop_duplicates()
#     df["ts"] = df["ts"].dt.tz_localize(None)
#     df.set_index("ts", inplace=True)
#     df_osm = pd.concat([df_osm, df.loc[:, ["lon", "lat"]]], ignore_index=False, axis=0)
#
# start_time = "2014-01-01 08:00:00"
# end_time = "2024-01-01 08:00:00"
# df_osm = df_osm.sort_index().loc[start_time:end_time, :]
# print(df_osm.head())
#
# for year, group in df_osm.groupby(df_osm.index.year):
#     filename = f"/data/zmen002/kdtree/real_world/osm/year/osm_{year}.csv"
#     length = group.index.size
#     group.loc[:, ["lon", "lat"]].to_csv(
#         filename, index=False, sep=" ", header=[length, 2]
#     )
