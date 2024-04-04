import osmium as osm
import pandas as pd

# https://github.com/osmcode/pyosmium/blob/master/examples/amenity_list.py


class OSMHandler(osm.SimpleHandler):
    def __init__(self):
        osm.SimpleHandler.__init__(self)
        self.osm_data = []

    def tag_inventory(self, elem, elem_type):
        for tag in elem.tags:
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


osmhandler = OSMHandler()
# scan the input file and fills the handler list accordingly
osmhandler.apply_file("../benchmark/real_world/antarctica-latest.osm.pbf")

# transform the list into a pandas DataFrame
data_colnames = [
    "id",
    "ts",
    "lon",
    "lat",
]
df = pd.DataFrame(osmhandler.osm_data, columns=data_colnames)
df = df.drop_duplicates()
df["ts"] = df["ts"].dt.tz_localize(None)
df.set_index("ts", inplace=True)
print(df.dtypes)

start_time = "2014-04-02 08:00:00"
end_time = "2024-04-02 08:00:00"
df = df.sort_index().loc[start_time:end_time, :]

print(len(df.index))
print(df.head())
df.to_csv("osm_data.csv")
