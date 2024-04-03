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
                    elem_type,
                    elem.id,
                    elem.version,
                    elem.visible,
                    pd.Timestamp(elem.timestamp),
                    elem.uid,
                    elem.user,
                    elem.changeset,
                    len(elem.tags),
                    tag.k,
                    tag.v,
                    elem.location.lon,
                    elem.location.lat,
                ]
            )

    def node(self, n):
        self.tag_inventory(n, "node")

    # def way(self, w):
    #     self.tag_inventory(w, "way")
    #
    # def relation(self, r):
    #     self.tag_inventory(r, "relation")


osmhandler = OSMHandler()
# scan the input file and fills the handler list accordingly
osmhandler.apply_file("antarctica-latest.osm.pbf")

# transform the list into a pandas DataFrame
data_colnames = [
    "type",
    "id",
    "version",
    "visible",
    "ts",
    "uid",
    "user",
    "chgset",
    "ntags",
    "tagkey",
    "tagvalue",
    "lon",
    "lat",
]
df_osm = pd.DataFrame(osmhandler.osm_data, columns=data_colnames)
# df_osm = tag_genome.sort_values(by=["type", "id", "ts"])

print(len(df_osm.index))
print(df_osm)
df_osm.to_csv("osm_data.csv")
