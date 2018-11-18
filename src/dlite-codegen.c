#include <assert.h>
#include <stdio.h>
#include <ctype.h>

#include "err.h"
#include "tgen.h"

#include "dlite-macros.h"
#include "dlite.h"



/* Context for looping over properties */
typedef struct {
  const DLiteInstance *inst;  /* pointer to the instance */
  int iprop;                  /* index of current property or -1 */
}  Context;


/* Generator function that simply copies the template.

   This might be useful to e.g. apply formatting. Should it be a part of
   tgen?
*/
static int copy(TGenBuf *s, const char *template, int len,
		const TGenSubs *subs, void *context)
{
  return tgen_append(s, template, len, subs, context);
}


/* Generator function for listing dimensions. */
static int list_dimensions(TGenBuf *s, const char *template, int len,
                           const TGenSubs *subs, void *context)
{
  int retval = 0;
  DLiteMeta *meta = (DLiteMeta *)((Context *)context)->inst;
  TGenSubs dsubs;
  size_t i;
  if (!dlite_meta_is_metameta(meta->meta))
    return err(TGenSyntaxError,
               "\"list_dimensions\" is only works for metadata");

  if ((retval = tgen_subs_copy(&dsubs, subs))) goto fail;
  for (i=0; i < meta->ndimensions; i++) {
    DLiteDimension *d = meta->dimensions + i;
    tgen_subs_set(&dsubs, "dim.name", d->name, NULL);
    tgen_subs_set(&dsubs, "dim.descr", d->description, NULL);
    tgen_subs_set_fmt(&dsubs, "dim.value", NULL, "%lu", DLITE_DIM(meta, i));
    tgen_subs_set_fmt(&dsubs, "dim.i",     NULL, "%lu", i);
    tgen_subs_set(&dsubs, ",",  (i < meta->ndimensions-1) ? ","  : "", NULL);
    tgen_subs_set(&dsubs, ", ", (i < meta->ndimensions-1) ? ", " : "", NULL);
    if ((retval = tgen_append(s, template, len, &dsubs, context))) goto fail;
  }
 fail:
  tgen_subs_deinit(&dsubs);
  return retval;
}

/* Generator function for listing relations. */
static int list_relations(TGenBuf *s, const char *template, int len,
                          const TGenSubs *subs, void *context)
{
  int retval = 0;
  DLiteMeta *meta = (DLiteMeta *)((Context *)context)->inst;
  TGenSubs rsubs;
  size_t i;
  if (!dlite_meta_is_metameta(meta->meta))
    return err(TGenSyntaxError,
               "\"list_relations\" is only works for metadata");

  if ((retval = tgen_subs_copy(&rsubs, subs))) goto fail;
  for (i=0; i < meta->nrelations; i++) {
    DLiteRelation *r = meta->relations + i;
    tgen_subs_set(&rsubs, "rel.s",  r->s, NULL);
    tgen_subs_set(&rsubs, "rel.p",  r->p, NULL);
    tgen_subs_set(&rsubs, "rel.o",  r->o, NULL);
    tgen_subs_set(&rsubs, "rel.id", r->id, NULL);
    tgen_subs_set_fmt(&rsubs, "rel.i", NULL, "%lu", i);
    tgen_subs_set(&rsubs, ",",  (i < meta->nrelations-1) ? ","  : "", NULL);
    tgen_subs_set(&rsubs, ", ", (i < meta->nrelations-1) ? ", " : "", NULL);
    if ((retval = tgen_append(s, template, len, &rsubs, context))) goto fail;
  }
 fail:
  tgen_subs_deinit(&rsubs);
  return retval;
}


/* Generator function for listing property dimensions. */
static int list_dims(TGenBuf *s, const char *template, int len,
                     const TGenSubs *subs, void *context)
{
  int retval = 1;
  DLiteMeta *meta = (DLiteMeta *)((Context *)context)->inst;
  int iprop = ((Context *)context)->iprop;
  DLiteDimension *dims = meta->dimensions;
  DLiteProperty *p = meta->properties + iprop;
  TGenSubs psubs;
  int i;
  if (!dlite_meta_is_metameta(meta->meta))
    return err(TGenSyntaxError,
               "\"list_dims\" only works for metadata");

  if (tgen_subs_copy(&psubs, subs)) goto fail;
  for (i=0; i < p->ndims; i++) {
    int dim = p->dims[i];
    tgen_subs_set(&psubs, "dim.name",  dims[dim].name,        NULL);
    tgen_subs_set(&psubs, "dim.descr", dims[dim].description, NULL);
    tgen_subs_set_fmt(&psubs, "dim.value", NULL, "%lu", DLITE_DIM(meta, dim));
    tgen_subs_set_fmt(&psubs, "dim.n",     NULL, "%d",  dim);
    tgen_subs_set_fmt(&psubs, "dim.i",     NULL, "%d",  i);
    tgen_subs_set(&psubs, ",",  (i < p->ndims-1) ? ","  : "", NULL);
    tgen_subs_set(&psubs, ", ", (i < p->ndims-1) ? ", " : "", NULL);
    if ((retval = tgen_append(s, template, len, &psubs, context))) goto fail;
  }
  retval = 0;
 fail:
  tgen_subs_deinit(&psubs);
  return retval;
}


/* Help function for list_properties.  If `metameta` is non-zero,
   `subs` is assigned the properties of `meta->meta`, otherwise it is
   assigned the properties of `meta`. Returns non-zero on error. */
static int list_properties_helper(TGenBuf *s, const char *template, int len,
                                  const TGenSubs *subs, void *context,
                                  int metameta)
{
  int retval = 0;
  const DLiteMeta *meta = (const DLiteMeta *)((Context *)context)->inst;
  const DLiteMeta *m = (metameta) ? meta->meta : meta;
  TGenSubs psubs;
  char *meta_name=NULL;
  size_t i;
  if (!dlite_meta_is_metameta(meta->meta))
    return err(TGenSyntaxError,
               "\"list_properties\" is only works for metadata");

  if (metameta)
    dlite_split_meta_uri(meta->uri, &meta_name, NULL, NULL);

  if ((retval = tgen_subs_copy(&psubs, subs))) goto fail;

  for (i=0; i < m->nproperties; i++) {
    DLiteProperty *p = m->properties + i;
    const char *type = dlite_type_get_dtypename(p->type);
    const char *dtype = dlite_type_get_enum_name(p->type);
    char *unit = (p->unit) ? p->unit : "";
    char *descr = (p->description) ? p->description : "";
    size_t nref = (p->ndims > 0) ? 1 : 0;
    int isallocated = dlite_type_is_allocated(p->type);
    char typename[32], cdecl[32];
    dlite_type_set_typename(p->type, p->size, typename, sizeof(typename));
    dlite_type_set_cdecl(p->type, p->size, p->name, nref,
                         cdecl, sizeof(cdecl));

    ((Context *)context)->iprop = i;
    tgen_subs_set(&psubs, "prop.name",     p->name,  NULL);
    tgen_subs_set(&psubs, "prop.type",     type,     NULL);
    tgen_subs_set(&psubs, "prop.typename", typename, NULL);
    tgen_subs_set(&psubs, "prop.dtype",    dtype,    NULL);
    tgen_subs_set(&psubs, "prop.cdecl",    cdecl,    NULL);
    tgen_subs_set(&psubs, "prop.unit",     unit,     NULL);
    tgen_subs_set(&psubs, "prop.descr",    descr,    NULL);
    tgen_subs_set(&psubs, "prop.dims",     NULL,     list_dims);
    tgen_subs_set_fmt(&psubs, "prop.typeno",      NULL, "%d",  p->type);
    tgen_subs_set_fmt(&psubs, "prop.size",        NULL, "%lu", p->size);
    tgen_subs_set_fmt(&psubs, "prop.ndims",       NULL, "%d",  p->ndims);
    tgen_subs_set_fmt(&psubs, "prop.isallocated", NULL, "%d",  isallocated);
    tgen_subs_set_fmt(&psubs, "prop.i",           NULL, "%lu", i);
    tgen_subs_set(&psubs, ",",  (i < m->nproperties-1) ? ","  : "", NULL);
    tgen_subs_set(&psubs, ", ", (i < m->nproperties-1) ? ", " : "", NULL);

    if (metameta) {
      if (p->ndims == 0 && p->type == dliteStringPtr) {
        char **ptr = dlite_instance_get_property((DLiteInstance *)meta,
                                                 p->name);
        tgen_subs_set_fmt(&psubs, "prop.value",  NULL, "%s", *ptr);
        tgen_subs_set_fmt(&psubs, "prop.cvalue", NULL, "\"%s\"", *ptr);
      } else {
        const TGenSub *sub;
        tgen_subs_set_fmt(&psubs, "prop.value", NULL, "%s_%s",
                          meta_name, p->name);
        tgen_subs_set_fmt(&psubs, "prop.cvalue", NULL, "%s_%s",
                          meta_name, p->name);
        sub = tgen_subs_get(&psubs, "prop.value");
        tgen_setcase(sub->repl, -1, 'l');
      }
    }

    if ((retval = tgen_append(s, template, len, &psubs, context)))
      goto fail;
  }
 fail:
  ((Context *)context)->iprop = -1;
  tgen_subs_deinit(&psubs);
  if (meta_name) free(meta_name);
  return retval;
}

/* Generator function for listing properties. */
static int list_properties(TGenBuf *s, const char *template, int len,
                           const TGenSubs *subs, void *context)
{
  return list_properties_helper(s, template, len, subs, context, 0);
}

/* Generator function for listing metadata properties. */
static int list_meta_dimensions(TGenBuf *s, const char *template, int len,
                                const TGenSubs *subs, void *context)
{
  Context c;
  c.inst = (DLiteInstance *)((Context *)context)->inst->meta;
  c.iprop = ((Context *)context)->iprop;
  return list_dimensions(s, template, len, subs, &c);
}

/* Generator function for listing metadata properties. */
static int list_meta_properties(TGenBuf *s, const char *template, int len,
                                const TGenSubs *subs, void *context)
{
  return list_properties_helper(s, template, len, subs, context, 1);
}

/* Generator function for listing metadata properties. */
static int list_meta_relations(TGenBuf *s, const char *template, int len,
                                const TGenSubs *subs, void *context)
{
  Context c;
  c.inst = (DLiteInstance *)((Context *)context)->inst->meta;
  c.iprop = ((Context *)context)->iprop;
  return list_relations(s, template, len, subs, &c);
}



/*
  Assign/update substitutions based on the instance `inst`.

  Returns non-zero on error.
*/
int instance_subs(TGenSubs *subs, const DLiteInstance *inst)
{
  char *name, *version, *namespace, **descr;
  const DLiteMeta *meta = inst->meta;
  int isdata=0, ismeta=0, ismetameta=0;
  size_t i;
  char basename[256];

  /* Determine what this data is */
  if (dlite_meta_is_metameta(meta)) {
    ismeta = 1;
    if (dlite_meta_is_metameta((DLiteMeta *)inst)) ismetameta = 1;
  } else {
    isdata = 1;
  }
  tgen_subs_set_fmt(subs, "isdata", NULL, "%d", isdata);
  tgen_subs_set_fmt(subs, "ismeta", NULL, "%d", ismeta);
  tgen_subs_set_fmt(subs, "ismetameta", NULL, "%d", ismetameta);

  /* General (all types of instances) */
  tgen_subs_set_fmt(subs, "uuid", NULL, "\"%s\"", inst->uuid);
  if (inst->uri)
    tgen_subs_set(subs, "uri",        inst->uri,  NULL);

  /* About metadata */
  dlite_split_meta_uri(meta->uri, &name, &version, &namespace);
  descr = dlite_instance_get_property((DLiteInstance *)meta, "description");
  tgen_subs_set(subs, "meta.uuid",       meta->uuid, NULL);
  tgen_subs_set(subs, "meta.uri",        meta->uri,  NULL);
  tgen_subs_set(subs, "meta.name",       name,       NULL);
  tgen_subs_set(subs, "meta.version",    version,    NULL);
  tgen_subs_set(subs, "meta.namespace",  namespace,  NULL);
  tgen_subs_set(subs, "meta.descr",      *descr,     NULL);

  /* DLiteInstance_HEAD */
  tgen_subs_set(subs, "_uuid", inst->uuid, NULL);
  tgen_subs_set(subs, "_uri", (inst->uri) ? inst->uri : "NULL", NULL);
  tgen_subs_set(subs, "_refcount", "0",    NULL);
  tgen_subs_set(subs, "_meta",     "NULL", NULL);

  /* For all metadata  */
  if (dlite_meta_is_metameta(inst->meta)) {
    DLiteMeta *meta = (DLiteMeta *)inst;
    dlite_split_meta_uri(inst->uri, &name, &version, &namespace);
    descr = dlite_instance_get_property((DLiteInstance *)meta, "description");
    tgen_subs_set(subs, "name",       name,       NULL);
    tgen_subs_set(subs, "version",    version,    NULL);
    tgen_subs_set(subs, "namespace",  namespace,  NULL);
    tgen_subs_set(subs, "descr",      *descr,     NULL);

  /* DLiteMeta_HEAD */
    tgen_subs_set_fmt(subs, "_ndimensions", NULL, "%lu", meta->ndimensions);
    tgen_subs_set_fmt(subs, "_nproperties", NULL, "%lu", meta->nproperties);
    tgen_subs_set_fmt(subs, "_nrelations",  NULL, "%lu", meta->nrelations);

    tgen_subs_set_fmt(subs, "_headersize",  NULL, "0");
    tgen_subs_set_fmt(subs, "_init",        NULL, "NULL");
    tgen_subs_set_fmt(subs, "_deinit",      NULL, "NULL");

    tgen_subs_set_fmt(subs, "_dimoffset",   NULL, "0");
    tgen_subs_set_fmt(subs, "_propoffsets", NULL, "NULL");
    tgen_subs_set_fmt(subs, "_reloffset",   NULL, "0");
    tgen_subs_set_fmt(subs, "_pooffset",    NULL, "0");
  }

  /* Lists */
  tgen_subs_set(subs, "list_dimensions", NULL, list_dimensions);
  tgen_subs_set(subs, "list_properties", NULL, list_properties);
  tgen_subs_set(subs, "list_relations",  NULL, list_relations);
  tgen_subs_set(subs, "list_meta_dimensions", NULL, list_meta_dimensions);
  tgen_subs_set(subs, "list_meta_properties", NULL, list_meta_properties);
  tgen_subs_set(subs, "list_meta_relations",  NULL, list_meta_relations);
  tgen_subs_set(subs, ".copy", NULL, copy);

  /* General */
  basename[0] = '\0';
  snprintf(basename, sizeof(basename), "%s", name);
  for (i=0; i < strlen(basename); i++)
    basename[i] = tolower(basename[i]);
  tgen_subs_set(subs, "basename", basename, NULL);

  return 0;
}


/*
  Assign/update substitutions based on `options`.

  Returns non-zero on error.
*/
int option_subs(TGenSubs *subs, const char *options)
{
  const char *v, *k = options;
  while (*k && *k != '#') {
    size_t vlen, klen = strcspn(k, "=;&#");
    if (k[klen] != '=')
      return errx(1, "no value for key '%.*s' in option string '%s'",
                  (int)klen, k, options);
    v = k + klen + 1;
    vlen = strcspn(v, ";&#");
    tgen_subs_setn_fmt(subs, k, klen, NULL, "%.*s", vlen, v);
    k = v + vlen;
    if (*k) k++;
  }
  return 0;
}


/*
  Returns a newly malloc'ed string with a generated document based on
  `template` and instanse `inst`.  `options` is a semicolon (;) separated
  string with additional options.

  Returns NULL on error.
 */
char *dlite_codegen(const char *template, const DLiteInstance *inst,
                    const char *options)
{
  TGenSubs subs;
  char *text;
  Context context;

  context.inst = inst;
  context.iprop = -1;

  tgen_subs_init(&subs);
  if (instance_subs(&subs, inst)) return NULL;
  if (option_subs(&subs, options)) return NULL;
  text = tgen(template, &subs, &context);
  tgen_subs_deinit(&subs);
  return text;
}
