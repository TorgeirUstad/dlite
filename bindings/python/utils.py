import binascii
import os

import dlite

thisdir = os.path.dirname(__file__)


def instance_from_dict(d):
    """Returns a new DLite instance created from dict `d`, which should
    be of the same form as returned by the Instance.asdict() method.
    """
    meta = dlite.get_instance(d['meta'])
    if meta.is_metameta:

        if 'uri' in d:
            uri = d['uri']
        else:
            uri = dlite.join_meta_uri(d['name'], d['version'], d['namespace'])

        try:
            with dlite.silent:
                inst = dlite.get_instance(uri)
                if inst:
                    return inst
        except dlite.DLiteError:
            pass

        dimensions = [dlite.Dimension(d['name'], d.get('description'))
                      for d in d['dimensions']]
        props = []
        for p in d['properties']:
            props.append(dlite.Property(
                name=p['name'],
                type=p['type'],
                dims=p.get('dims'),
                unit=p.get('unit'),
                iri=p.get('iri'),
                description=p.get('description')))
        inst = dlite.Instance(uri, dimensions, props, d.get('iri'),
                              d.get('description'))
    else:
        dims = list(d['dimensions'].values())
        if 'uri' in d.keys():
            arg = d.get('uri', d.get('uuid', None))
        else:
            arg = d.get('uuid', None)
        inst = dlite.Instance(meta.uri, dims, arg)
        for p in meta['properties']:
            value = d['properties'][p.name]
            if p.type.startswith('blob') and type(value) == str:
                # If binary data is a string, assume it is hexadecimal
                value = bytearray(binascii.unhexlify(value))
            inst[p.name] = value
    return inst


def get_package_paths():
    return {k:v for k,v in dlite.__dict__.items() if k.endswith('path')}


if __name__ == '__main__':

    url = 'json://' + os.path.join(thisdir, 'tests', 'Person.json')
    Person = dlite.Instance(url)

    person = Person([2])
    person.name = 'Ada'
    person.age = 12.5
    person.skills = ['skiing', 'jumping']

    d1 = person.asdict()
    inst1 = instance_from_dict(d1)

    d2 = Person.asdict()
    inst2 = instance_from_dict(d2)
