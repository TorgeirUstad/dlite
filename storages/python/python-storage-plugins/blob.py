"""A dumb storage plugin that simply reads/writes a file to/from an instance
as a binary blob.

The generated entity has no dimensions and one property called "content".
"""
import numpy as np
import dlite


class blob(DLiteStorageBase):
    """DLite storage plugin for binary blobs."""

    def open(self, uri, options=None):
        """Opens `uri`."""
        self.uri = uri

    def close(self):
        """Closes this storage."""
        pass

    def load(self, uuid):
        """Loads `uuid` from current storage and return it as a new instance."""
        with open(self.uri, 'rb') as f:
            content = f.read()
        meta = dlite.get_instance('http://onto-ns.com/meta/0.1/Blob')
        inst = meta(dims=[len(content)])
        inst.content = np.frombuffer(content, dtype='uint8')
        return inst

    def save(self, inst):
        """Stores `inst` in current storage."""
        with open(self.uri, 'wb') as f:
            f.write(inst.content)
