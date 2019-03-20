#include <assert.h>
#include <string.h>

#include "utils/map.h"
#include "utils/tgen.h"

#include "dlite-macros.h"
#include "dlite-store.h"
#include "dlite-entity.h"
#include "dlite-mapping-plugins.h"
#include "dlite-mapping.h"


/*
  Custom map types.  Are also used as sets (unused map value).
 */
typedef map_t(DLiteInstance *) Instances;
typedef map_t(DLiteMapping *) Mappings;


/*
  Recursive help function that removes all mappings found in `m` and its
  submappings from `created`.
 */
void mapping_remove_rec(DLiteMapping *m, Mappings *created)
{
  int i;
  if (!m) return;
  map_remove(created, m->output_uri);
  for (i=0; i < m->ninput; i++)
    if (m->input_maps[i])
      mapping_remove_rec((DLiteMapping *)m->input_maps[i], created);
}

/*
  Recursive help function returning a mapping.

  `inputs` set of input URIs (unused map value)
  `visited` set of so far visited output URIs (unused map value)
  `created` maps all created output URIs to corresponding mapping
  `dead_ends` set of URIs that we cannot create a mapping to (unused map value)
 */
DLiteMapping *mapping_create_rec(const char *output_uri, Instances *inputs,
                                 Mappings *visited, Mappings *created,
                                 Mappings *dead_ends)
{
  int i, lowest_cost=-1;
  DLiteMapping *m=NULL, *retval=NULL;
  DLiteMappingPlugin *api, *cheapest=NULL;
  DLiteMappingPluginIter iter;

  dlite_mapping_plugin_init_iter(&iter);

  /* Ensure that no input URI equals output URI */
  assert(!map_get(inputs, output_uri));

  /* Ensure that this not an already known dead end */
  assert(!map_get(dead_ends, output_uri));

  /* Add output_uri to visited */
  assert(!map_get(visited, output_uri));
  map_set(visited, output_uri, NULL);

  /* Find cheapest mapping to output_api */
  while ((api = dlite_mapping_plugin_next(&iter))) {
    int ignore = 0;
    int cost = api->cost;
    if (strcmp(output_uri, api->output_uri) != 0) continue;

    printf("*** -> %s\n", output_uri);

    /* avoid infinite cyclic loops and known dead ends */
    for (i=0; i < api->ninput; i++) {

      printf("    : input_uris[%d]='%s'\n", i, api->input_uris[i]);

      if (map_get(visited, api->input_uris[i]) ||
          map_get(dead_ends, api->input_uris[i])) {
        ignore = 1;
        break;
      }
    }
    if (ignore) continue;

    /* avoid mappings that depends on input that cannot be realised and
       calculate cost */
    for (i=0; i < api->ninput; i++) {
      if (!map_get(inputs, api->input_uris[i])) {
        DLiteMapping *mapping=NULL, **mappingp=NULL;
        if (!(mappingp = map_get(created, api->input_uris[i])) &&
            !(mapping = mapping_create_rec(api->input_uris[i], inputs,
                                           visited, created, dead_ends))) {
          ignore = 1;
          break;
        } else {
          if (!mapping) mapping = *mappingp;
          assert(mapping->cost >= 0);
          cost += mapping->cost;
        }
      }
    }
    if (ignore) continue;

    if (!cheapest || cost < lowest_cost) {
      cheapest = api;
      lowest_cost = cost;
    }
  }
  if (!(api = cheapest)) goto fail;

  /* create mapping */
  assert(strcmp(output_uri, api->output_uri) == 0);
  if (!(m = calloc(1, sizeof(DLiteMapping)))) FAIL("allocation failure");
  m->name = api->name;
  m->output_uri = api->output_uri;
  m->ninput = api->ninput;
  if (!(m->input_maps = calloc(m->ninput, sizeof(DLiteMapping *))))
    FAIL("allocation failure");
  if (!(m->input_uris = calloc(m->ninput, sizeof(char *))))
    FAIL("allocation failure");
  for (i=0; i < api->ninput; i++) {
    if (!map_get(inputs, api->input_uris[i])) {
      DLiteMapping **p = map_get(created, api->input_uris[i]);
      assert(p);
      m->input_maps[i] = *p;
      assert(m->input_maps[i]);
    } else
      m->input_uris[i] = api->input_uris[i];
  }
  m->api = api;
  m->cost = lowest_cost;

  map_set(created, output_uri, m);

  retval = m;
 fail:
  map_remove(visited, output_uri);
  if (!retval) map_set(dead_ends, output_uri, NULL);
  return retval;
}


/*
  Returns a new nested mapping structure describing how the set of
  input URIs in `inputs` can be mapped to `output_uri`.
 */
DLiteMapping *mapping_create_base(const char *output_uri, Instances *inputs)
{
  Mappings visited, created, dead_ends;
  DLiteMapping *m=NULL, *retval=NULL;
  map_iter_t iter;
  const char *key;

  map_init(&visited);
  map_init(&created);
  map_init(&dead_ends);

  if ((map_get(inputs, output_uri))) {
    /* The trivial case - one of the input URIs equals output URI. */
    if (!(m = calloc(1, sizeof(DLiteMapping))))
      FAIL("allocation failure");
    m->name = NULL;
    m->output_uri = output_uri;
    m->ninput = 1;
    if (!(m->input_maps = calloc(1, sizeof(DLiteMapping *))))
      FAIL("allocation failure");
    if (!(m->input_uris = calloc(1, sizeof(char *))))
      FAIL("allocation failure");
    m->input_uris[0] = output_uri;
    m->api = NULL;
    m->cost = 0;

  } else {
    m = mapping_create_rec(output_uri, inputs, &visited, &created, &dead_ends);
  }

  retval = m;
 fail:

  /* Free all created mappings not in retval */
  mapping_remove_rec(retval, &created);
  iter = map_iter(&created);
  while ((key = map_next(&created, &iter))) {
    DLiteMapping **mp = map_get(&created, key);
    assert(mp && *mp);
    free(*mp);
  }

  map_deinit(&visited);
  map_deinit(&created);
  map_deinit(&dead_ends);
  if (!retval && m) dlite_mapping_free(m);
  return retval;
}


/*
  Returns a new nested mapping structure describing how `n` input
  instances of metadata `input_uris` can be mapped to `output_uri`.

  Note, in the trivial case where one of the input URIs equals `output_uri`,
  will the "output_uri" field in the returned mapping point to `output_uri`.
  Hence, do not free `output_uri` as long as the returned mapping is in use.
 */
DLiteMapping *dlite_mapping_create(const char *output_uri,
                                   const char **input_uris, int n)
{
  int i;
  Instances inputs;
  DLiteMapping *m=NULL;

  map_init(&inputs);

  /* Check that all input_uris are unique */
  for (i=0; i<n; i++) {
    if (map_get(&inputs, input_uris[i]))
      FAIL1("more than one mapping input of the same metadata: %s",
            input_uris[i]);
    map_set(&inputs, input_uris[i], NULL);
  }

  m = mapping_create_base(output_uri, &inputs);
 fail:
  map_deinit(&inputs);
  return m;
}


/*
  Frees a nested mapping tree.
*/
void dlite_mapping_free(DLiteMapping *m)
{
  int i;
  for (i=0; i < m->ninput; i++) {
    assert(m->input_maps[i] || m->input_uris[i]);
    assert(!(m->input_maps[i] && m->input_uris[i]));
    if (m->input_maps[i]) dlite_mapping_free((DLiteMapping *)m->input_maps[i]);
  }
  free(m->input_maps);
  free(m->input_uris);
  free(m);
}


/*
  Recursive help function that performs the actual mapping and returns
  a new instance (with metadata `m->output_uri`).

  `m` mapping tree that descripes how the instance can be created.
  `instances` maps metadata URI to an instance with this metadata.  The
      instance should be either an input instance or created by a
      mapping.
 */
DLiteInstance *mapping_map_rec(const DLiteMapping *m, Instances *instances)
{
  int i;
  DLiteInstance *inst=NULL, **insts=NULL, **instp;

  /* Trivial case - we already have an instance with metadata `m->output_uri` */
  if ((instp = map_get(instances, m->output_uri)))
    return *instp;

  /* Create `insts` array */
  if (!(insts = calloc(m->ninput, sizeof(DLiteInstance))))
    FAIL("allocation failure");
  for (i=0; i < m->ninput; i++) {
    if (m->input_maps[i]) {
      insts[i] = mapping_map_rec(m->input_maps[i], instances);
    } else {
      instp = map_get(instances, m->input_uris[i]);

      printf("*** input_uris[%d]='%s'\n", i, m->input_uris[i]);

      assert(instp);
      insts[i] = *instp;
    }
  }

  /* Call the mapper function from plugin */
  if (!(inst = m->api->mapper(insts, m->ninput))) goto fail;

  /* Add new instance to `instances` */
  assert(strcmp(inst->meta->uri, m->output_uri) == 0);
  map_set(instances, inst->meta->uri, inst);

 fail:
  if (insts) free(insts);
  return inst;
}


/*
  Recursive help function that appends to `s`.
 */
void mapping_string_rec(const DLiteMapping *m, TGenBuf *s, int indent)
{
  int i, j;
  for (j=0; j<indent-1; j++) tgen_buf_append_fmt(s, "|   ");
  if (indent) tgen_buf_append_fmt(s, "+-- ");
  tgen_buf_append_fmt(s, "%s\n", m->output_uri);

  if (!m->name) return;

  for (i=0; i < m->ninput; i++) {
    if (m->input_maps[i]) {
      mapping_string_rec(m->input_maps[i], s, indent+1);
    } else {
      for (j=0; j<indent; j++) tgen_buf_append_fmt(s, "|   ");
      tgen_buf_append_fmt(s, "+-- ");
      tgen_buf_append_fmt(s, "%s\n", m->input_uris[i]);
    }
  }
}


/*
  Returns a malloc'ed string displaying mapping `m`.
 */
char *dlite_mapping_string(const DLiteMapping *m)
{
  TGenBuf s;
  char *str=NULL;
  tgen_buf_init(&s);
  mapping_string_rec(m, &s, 0);
  str = strdup(tgen_buf_get(&s));
  tgen_buf_deinit(&s);
  return str;
}


/* Assign `inputs` from `instances`.  Returns non-zero on error. */
int set_inputs(Instances *inputs, const DLiteInstance **instances, int n)
{
  int i;
  for (i=0; i<n; i++) {
    const char *uri = instances[i]->meta->uri;
    if (map_get(inputs, uri))
      return err(1, "more than one instance of the same metadata: %s", uri);
    dlite_instance_incref((DLiteInstance *)instances[i]);
    map_set(inputs, uri, (DLiteInstance *)instances[i]);
  }
  return 0;
}


/*
  Applies the mapping `m` on `instances` (array of length `n` of
  instance pointers) and returns a new instance.
 */
DLiteInstance *dlite_mapping_map(const DLiteMapping *m,
                                 const DLiteInstance **instances, int n)
{
  const char *key;
  Instances inputs;
  map_iter_t iter;
  DLiteInstance *inst=NULL, **instp;

  map_init(&inputs);

  /* Assign instances and check that the metadata of all instances are unique */
  if (set_inputs(&inputs, instances, n)) goto fail;

  if ((instp = map_get(&inputs, m->output_uri))) {
    /* The trivial case - one of the inputs has metadata output URI */
    assert(!m->name);
    inst = *instp;
    assert(inst);
    dlite_instance_incref(inst);
  } else {
    /* Apply mapping */
    assert(m->name);  /* trivial case is already handled */
    inst = mapping_map_rec(m, &inputs);
  }

 fail:
  /* Remove temporary created instances */
  iter = map_iter(&inputs);
  while ((key = map_next(&inputs, &iter))) {
    DLiteInstance **ip = map_get(&inputs, key);
    assert(ip && *ip);
    dlite_instance_decref(*ip);
  }

  map_deinit(&inputs);
  return inst;
}


/*
  Returns a new instance of metadata `output_uri` by mapping the `n` input
  instances in the array `instances`.

  This is the main function in the mapping api.
 */
DLiteInstance *dlite_mapping(const char *output_uri,
                             const DLiteInstance **instances, int n)
{
  DLiteInstance *inst=NULL;
  DLiteMapping *m=NULL;
  Instances inputs;

  map_init(&inputs);

  if (set_inputs(&inputs, instances, n)) goto fail;
  if ((m = mapping_create_base(output_uri, &inputs))) goto fail;
  inst = dlite_mapping_map(m, instances, n);

 fail:
  map_deinit(&inputs);
  if (m) dlite_mapping_free(m);
  return inst;
}
