/**
 * \file dash/dart/base/internal/host_topology.c
 *
 */

/*
 * Include config and first to prevent previous include without _GNU_SOURCE
 * in included headers:
 */
#include <dash/dart/base/config.h>
#ifdef DART__PLATFORM__LINUX
#  define _GNU_SOURCE
#  include <unistd.h>
#endif

#include <string.h>
#include <limits.h>

#include <dash/dart/if/dart_types.h>
#include <dash/dart/if/dart_team_group.h>
#include <dash/dart/base/internal/host_topology.h>
#include <dash/dart/base/internal/unit_locality.h>
#include <dash/dart/base/internal/hwloc.h>

#include <dash/dart/base/logging.h>
#include <dash/dart/base/assert.h>

#ifdef DART_ENABLE_HWLOC
#  include <hwloc.h>
#  include <hwloc/helper.h>
#endif


/* ===================================================================== *
 * Private Functions                                                     *
 * ===================================================================== */

static int cmpstr_(const void * p1, const void * p2) {
  return strcmp(* (char * const *) p1, * (char * const *) p2);
}

/* ===================================================================== *
 * Internal Functions                                                    *
 * ===================================================================== */

dart_ret_t dart__base__host_topology__module_locations(
  dart_module_location_t ** module_locations,
  int                     * num_modules)
{
  *module_locations = NULL;
  *num_modules      = 0;

#ifdef DART_ENABLE_HWLOC
  DART_LOG_TRACE("dart__base__host_topology__module_locations: using hwloc");

  hwloc_topology_t topology;
  hwloc_topology_init(&topology);
  hwloc_topology_set_flags(topology,
  /*                         HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM */
#if HWLOC_API_VERSION < 0x00020000
                             HWLOC_TOPOLOGY_FLAG_IO_DEVICES
                           | HWLOC_TOPOLOGY_FLAG_IO_BRIDGES
  /*                       | HWLOC_TOPOLOGY_FLAG_WHOLE_IO  */
#endif
                          );
  hwloc_topology_load(topology);
  DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                 "hwloc: indexing PCI devices");
  /* Alternative: HWLOC_TYPE_DEPTH_PCI_DEVICE */
  int n_pcidev = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PCI_DEVICE);

  DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                 "hwloc: %d PCI devices found", n_pcidev);
  for (int pcidev_idx = 0; pcidev_idx < n_pcidev; pcidev_idx++) {
    hwloc_obj_t coproc_obj =
      hwloc_get_obj_by_type(topology, HWLOC_OBJ_PCI_DEVICE, pcidev_idx);
    if (NULL != coproc_obj) {
      DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                     "hwloc: PCI device: (name:%s arity:%d)",
                     coproc_obj->name, coproc_obj->arity);
      if (strstr(coproc_obj->name, "Xeon Phi") != NULL) {
        DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                       "hwloc: Xeon Phi device");
        if (coproc_obj->arity > 0) {
          for (int pd_i = 0; pd_i < coproc_obj->arity; pd_i++) {
            hwloc_obj_t coproc_child_obj = coproc_obj->children[pd_i];
            DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                           "hwloc: Xeon Phi child node: (name:%s arity:%d)",
                           coproc_child_obj->name, coproc_child_obj->arity);
            (*num_modules)++;
            *module_locations = realloc(*module_locations,
                                       *num_modules *
                                         sizeof(dart_module_location_t));
            dart_module_location_t * module_loc =
              module_locations[(*num_modules)-1];

            char * hostname     = module_loc->host;
            char * mic_hostname = module_loc->module;
            gethostname(hostname, DART_LOCALITY_HOST_MAX_SIZE);

            char * mic_dev_name = coproc_child_obj->name;
            strncpy(mic_hostname, hostname,     DART_LOCALITY_HOST_MAX_SIZE);
            strncat(mic_hostname, "-",          DART_LOCALITY_HOST_MAX_SIZE);
            strncat(mic_hostname, mic_dev_name, DART_LOCALITY_HOST_MAX_SIZE);
            DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                           "hwloc: Xeon Phi hostname: %s", mic_hostname);

            /* Get host of MIC device: */
            hwloc_obj_t mic_host_obj =
              hwloc_get_non_io_ancestor_obj(topology, coproc_obj);
            if (mic_host_obj != NULL) {

              module_loc->pos.scope =
                dart__base__hwloc__obj_type_to_dart_scope(mic_host_obj->type);
              module_loc->pos.index = mic_host_obj->logical_index;
              DART_LOG_TRACE("dart__base__host_topology__module_locations: "
                             "hwloc: Xeon Phi scope pos: "
                             "(type:%d -> scope:%d idx:%d)",
                             mic_host_obj->type,
                             module_loc->pos.scope,
                             module_loc->pos.index);
            }
          }
        }
      }
    }
  }
  hwloc_topology_destroy(topology);
#endif /* ifdef DART_ENABLE_HWLOC */
  return DART_OK;
}

dart_ret_t dart__base__host_topology__create(
  dart_unit_mapping_t   * unit_mapping,
  dart_host_topology_t ** host_topology)
{
  *host_topology   = NULL;
  dart_team_t team = unit_mapping->team;
  size_t num_units;

  DART_LOG_TRACE("dart__base__host_topology__create: team:%d", team);

  DART_ASSERT_RETURNS(dart_team_size(team, &num_units), DART_OK);
  DART_ASSERT_MSG(num_units == unit_mapping->num_units,
                  "Number of units in mapping differs from team size");

  /* Copy host names of all units into array:
   */
  const int max_host_len = DART_LOCALITY_HOST_MAX_SIZE;
  DART_LOG_TRACE("dart__base__locality__create: copying host names");
  char ** hostnames = malloc(sizeof(char *) * num_units);
  for (size_t u = 0; u < num_units; ++u) {
    hostnames[u] = malloc(sizeof(char) * max_host_len);
    dart_unit_locality_t * ul;
    DART_ASSERT_RETURNS(
      dart__base__unit_locality__at(unit_mapping, u, &ul),
      DART_OK);
    strncpy(hostnames[u], ul->hwinfo.host, max_host_len);
  }

  dart_host_topology_t * topo = malloc(sizeof(dart_host_topology_t));

  /*
   * Find unique host names in array 'hostnames' and count units per
   * host in same pass:
   */
  DART_LOG_TRACE("dart__base__host_topology__init: "
                 "filtering host names of %ld units", num_units);
  qsort(hostnames, num_units, sizeof(char*), cmpstr_);
  size_t last_host_idx  = 0;
  /* Maximum number of units mapped to a single host: */
  int    max_host_units = 0;
  /* Number of units mapped to current host: */
  int    num_host_units = 0;
  for (size_t u = 0; u < num_units; ++u) {
    ++num_host_units;
    if (u == last_host_idx) { continue; }
    /* copies next differing host name to the left, like:
     *
     *     [ a a a a b b b c c c ]  last_host_index++ = 1
     *         .-----'
     *         v
     * ->  [ a b a a b b b c c c ]  last_host_index++ = 2
     *           .---------'
     *           v
     * ->  [ a b c a b b b c c c ]  last_host_index++ = 3
     * ...
     */
    if (strcmp(hostnames[u], hostnames[last_host_idx]) != 0) {
      ++last_host_idx;
      strncpy(hostnames[last_host_idx], hostnames[u], max_host_len);
      if (num_host_units > max_host_units) {
        max_host_units = num_host_units;
      }
      num_host_units = 0;
    }
  }
  if (max_host_units == 0) {
    /* All units mapped to same host: */
    max_host_units = num_host_units;
  }
  /* All entries after index last_host_ids are duplicates now: */
  int num_hosts = last_host_idx + 1;
  DART_LOG_TRACE("dart__base__host_topology__init: number of hosts: %d",
                 num_hosts);
  DART_LOG_TRACE("dart__base__host_topology__init: max. number of units "
                 "mapped to a hosts: %d", max_host_units);

  /* Map units to hosts: */
  topo->node_units = malloc(num_hosts * sizeof(dart_node_units_t));
  for (int h = 0; h < num_hosts; ++h) {
    dart_node_units_t * node_units = &topo->node_units[h];
    /* Histogram of NUMA ids: */
    int numa_ids[DART_LOCALITY_MAX_NUMA_ID] = { 0 };
    /* Allocate array with capacity of maximum units on a single host: */
    node_units->units     = malloc(sizeof(dart_unit_t) * max_host_units);
    node_units->num_units = 0;
    node_units->num_numa  = 0;
    strncpy(node_units->host, hostnames[h], max_host_len);

    DART_LOG_TRACE("dart__base__host_topology__init: mapping units to %s",
                   hostnames[h]);
    /* Iterate over all units: */
    for (size_t u = 0; u < num_units; ++u) {
      dart_unit_locality_t * ul;
      DART_ASSERT_RETURNS(
        dart__base__unit_locality__at(unit_mapping, u, &ul),
        DART_OK);
      if (strncmp(ul->hwinfo.host, hostnames[h], max_host_len) == 0) {
        /* Unit is local to host at index h: */
        node_units->units[node_units->num_units] = ul->unit;
        node_units->num_units++;

        int unit_numa_id = ul->hwinfo.numa_id;

        DART_LOG_TRACE("dart__base__host_topology__init: "
                       "mapping unit %ld to host '%s', NUMA id: %d",
                       u, hostnames[h], unit_numa_id);
        if (unit_numa_id >= 0) {
          if (numa_ids[unit_numa_id] == 0) {
            node_units->num_numa++;
          }
          numa_ids[unit_numa_id]++;
        }
      }
    }
    DART_LOG_TRACE("dart__base__host_topology__init: "
                   "found %d NUMA domains on host %s",
                   node_units->num_numa, hostnames[h]);

#if 0
    if (node_units->num_numa < 1) {
      node_units->num_numa = 1;
    }
    for (int u = 0; u < node_units->num_units; ++u) {
      dart_unit_locality_t * ul;
      DART_ASSERT_RETURNS(
        dart__base__unit_locality__at(
          unit_mapping, node_units->units[u], &ul),
        DART_OK);
      ul->hwinfo.num_numa = node_units->num_numa;
    }
#endif

    /* Shrink unit array to required capacity: */
    if (node_units->num_units < max_host_units) {
      DART_LOG_TRACE("dart__base__host_topology__init: shrinking node unit "
                     "array from %d to %d elements",
                     max_host_units, node_units->num_units);
//    node_units->units = realloc(node_units->units, node_units->num_units);
      DART_ASSERT(node_units->units != NULL);
    }
  }

  /*
   * Initiate all-to-all exchange of module locations like Xeon Phi
   * hostnames and their assiocated NUMA domain in their parent node.
   *
   * Select one leader unit per node for communication:
   */
  dart_unit_locality_t * my_uloc;
  dart_unit_t            my_id;
  dart_unit_t            leader_unit_id;
  DART_ASSERT_RETURNS(
    dart_team_myid(unit_mapping->team, &my_id),
    DART_OK);
  DART_ASSERT_RETURNS(
    dart__base__unit_locality__at(
      unit_mapping, my_id, &my_uloc),
    DART_OK);
  for (int h = 0; h < num_hosts; ++h) {
    /* Get unit ids at local unit's host */
    if (strncmp(my_uloc->hwinfo.host, hostnames[h], max_host_len) == 0) {
      dart_node_units_t * node_units = &topo->node_units[h];
      /* Select first unit id at local host as leader: */
      leader_unit_id = node_units->units[0];
      break;
    }
  }
  DART_LOG_TRACE("dart__base__host_topology__init: "
                 "leader unit on host %s: %d",
                 my_uloc->hwinfo.host, leader_unit_id);
  /*
   * TODO: Find method to reduce communication to leaders
   */
  if (1 || my_id == leader_unit_id) {
    dart_module_location_t * module_locations;
    int                      num_modules;
    dart__base__host_topology__module_locations(
      &module_locations, &num_modules);
    for (int m = 0; m < num_modules; ++m) {
      /* iterate modules in outer loop to short-circuit for num_modules = 0 */
      for (int h = 0; h < num_hosts; ++h) {
        if (strncmp(module_locations[m].module, hostnames[h], max_host_len)
            == 0) {
          DART_LOG_TRACE("dart__base__host_topology__init: "
                         "module %s located at parent host %s "
                         "in scope:%d at rel.idx:d%",
                         module_locations[m].module,
                         module_locations[m].host,
                         module_locations[m].pos.scope,
                         module_locations[m].pos.index);
          dart_node_units_t * module_node_units = &topo->node_units[h];
          strncpy(module_node_units->parent, module_locations[m].host,
                  DART_LOCALITY_HOST_MAX_SIZE);
        }
      }
    }
  }

  topo->num_hosts  = num_hosts;
//topo->host_names = (char **)(realloc(hostnames, num_hosts * sizeof(char*)));
  topo->host_names = hostnames;
  DART_ASSERT(topo->host_names != NULL);

  /* Classify hostnames into categories 'node' and 'module'.
   * Typically, modules have the hostname of their nodes as prefix in their
   * hostname, e.g.:
   *
   *   computer-node-124           <-- node, heterogenous
   *   |- compute_node-124-sys     <-- module, homogenous
   *   |- compute-node-124-mic0    <-- module, homogenous
   *   '- compute-node-124-mic1    <-- module, homogenous
   *
   * Find shortest strings in array of distinct host names:
   */
  int hostname_min_len = INT_MAX;
  int hostname_max_len = 0;
  for (int n = 0; n < num_hosts; ++n) {
    topo->node_units[n].level     = 0;
    topo->node_units[n].parent[0] = '\0';
    int hostname_len = strlen(topo->host_names[n]);
    if (hostname_len < hostname_min_len) {
      hostname_min_len = hostname_len;
    }
    if (hostname_len > hostname_max_len) {
      hostname_max_len = hostname_len;
    }
  }
  DART_LOG_TRACE("dart__base__host_topology__init: "
                 "host name length min: %d, max: %d",
                 hostname_min_len, hostname_max_len);

  topo->num_host_levels = 0;
  topo->num_nodes       = num_hosts;
  if (hostname_min_len != hostname_max_len) {
    topo->num_nodes = 0;
    int num_modules = 0;
    /* Match short hostnames as prefix of every other hostname: */
    for (int top = 0; top < num_hosts; ++top) {
      if (strlen(topo->host_names[top]) == (size_t)hostname_min_len) {
        ++topo->num_nodes;
        /* Host name is node, find its modules in all other hostnames: */
        char * short_name = topo->host_names[top];
        DART_LOG_TRACE("dart__base__host_topology__init: node: %s",
                       short_name);
        for (int sub = 0; sub < num_hosts; ++sub) {
          char * other_name = topo->host_names[sub];
          /* Other hostname is longer and has short host name in prefix: */
          if (strlen(other_name) > (size_t)hostname_min_len &&
              strncmp(short_name, other_name, hostname_min_len) == 0) {
            DART_LOG_TRACE("dart__base__host_topology__init: "
                           "module: %s, parent node: %s",
                           other_name, short_name);
            num_modules++;
            /* Increment topology level of other host: */
            int node_level = topo->node_units[top].level + 1;
            if (node_level > topo->num_host_levels) {
              topo->num_host_levels = node_level;
            }
            topo->node_units[sub].level = node_level;
            /* Set short hostname as parent: */
            strncpy(topo->node_units[sub].parent, short_name,
                    DART_LOCALITY_HOST_MAX_SIZE);
          }
        }
      }
    }
    if (num_hosts > topo->num_nodes + num_modules) {
      /* some hosts are modules of node that is not in host names: */
      topo->num_nodes += num_hosts - (topo->num_nodes + num_modules);
    }
    DART_LOG_TRACE("dart__base__host_topology__init: "
                   "hosts: %d nodes: %d modules: %d",
                   topo->num_hosts, topo->num_nodes, num_modules);
  }
  *host_topology = topo;
  return DART_OK;
}

dart_ret_t dart__base__host_topology__destruct(
  dart_host_topology_t * topo)
{
  DART_LOG_DEBUG("dart__base__host_topology__destruct()");
  if (topo->node_units != NULL) {
    free(topo->node_units);
    topo->node_units = NULL;
  }
  if (topo->host_names != NULL) {
    free(topo->host_names);
    topo->host_names = NULL;
  }
  DART_LOG_DEBUG("dart__base__host_topology__destruct >");
  return DART_OK;
}


dart_ret_t dart__base__host_topology__num_nodes(
  dart_host_topology_t  * topo,
  int                   * num_nodes)
{
  *num_nodes = topo->num_nodes;
  return DART_OK;
}

dart_ret_t dart__base__host_topology__node(
  dart_host_topology_t  * topo,
  int                     node_index,
  const char           ** node_hostname)
{
  int n_index = 0;
  for (int h = 0; h < topo->num_hosts; ++h) {
    if (topo->node_units[h].level == 0) {
      if (n_index == node_index) {
        *node_hostname = topo->host_names[h];
        return DART_OK;
      }
      n_index++;
    }
  }
  return DART_ERR_NOTFOUND;
}

dart_ret_t dart__base__host_topology__num_node_modules(
  dart_host_topology_t  * topo,
  const char            * node_hostname,
  int                   * num_modules)
{
  *num_modules = 0;
  for (int h = 0; h < topo->num_hosts; ++h) {
    /* also includes node itself */
    char * m_hostname = topo->node_units[h].host;
    if (strncmp(node_hostname, m_hostname, strlen(node_hostname)) == 0) {
      *num_modules += 1;
    }
  }
  return DART_OK;
}

dart_ret_t dart__base__host_topology__node_module(
  dart_host_topology_t  * topo,
  const char            * node_hostname,
  int                     module_index,
  const char           ** module_hostname)
{
  int m_index = 0;
  for (int h = 0; h < topo->num_hosts; ++h) {
    char * m_hostname = topo->host_names[h];
    /* also includes node itself */
    if (strncmp(node_hostname, m_hostname, strlen(node_hostname)) == 0) {
      if (m_index == module_index) {
        *module_hostname = m_hostname;
        return DART_OK;
      }
      m_index++;
    }
  }
  return DART_ERR_NOTFOUND;
}

dart_ret_t dart__base__host_topology__node_units(
  dart_host_topology_t  * topo,
  const char            * hostname,
  dart_unit_t          ** units,
  int                   * num_units)
{
  DART_LOG_TRACE("dart__base__host_topolgoy__node_units() host: %s",
                 hostname);
  *num_units     = 0;
  *units         = NULL;
  int host_found = 0;
  /*
   * Also includes units in sub-modules, e.g. a query for host name
   * "some-node" would also include units from "sub-node-*":
   */

  /* First pass: Find total number of units: */
  for (int h = 0; h < topo->num_hosts; ++h) {
    dart_node_units_t * node_units = &topo->node_units[h];
    if (strncmp(node_units->host, hostname, strlen(hostname))
        == 0) {
      *num_units += node_units->num_units;
      host_found  = 1;
    }
  }
  if (!host_found) {
    DART_LOG_ERROR("dart__base__host_topology__node_units ! "
                   "no entry for host '%s')", hostname);
    return DART_ERR_NOTFOUND;
  }
  /* Second pass: Copy unit ids: */
  dart_unit_t * node_unit_ids = malloc(*num_units * sizeof(dart_unit_t));
  int           node_unit_idx = 0;
  for (int h = 0; h < topo->num_hosts; ++h) {
    dart_node_units_t * node_units = &topo->node_units[h];
    if (strncmp(node_units->host, hostname, strlen(hostname))
        == 0) {
      for (int nu = 0; nu < node_units->num_units; ++nu) {
        node_unit_ids[node_unit_idx + nu] = node_units->units[nu];
      }
      node_unit_idx += node_units->num_units;
    }
  }
  *units = node_unit_ids;
  DART_LOG_TRACE("dart__base__host_topology__node_units > num_units: %d",
                 *num_units);
  return DART_OK;
}

dart_ret_t dart__base__host_topology__module_units(
  dart_host_topology_t  * topo,
  const char            * hostname,
  dart_unit_t          ** units,
  int                   * num_units,
  int                   * num_numa_domains)
{
  DART_LOG_TRACE("dart__base__host_topolgoy__module_units() host: %s",
                 hostname);
  *num_units        = 0;
  *num_numa_domains = 0;
  *units            = NULL;
  int host_found    = 0;
  /*
   * Does not include units in sub-modules, e.g. a query for host name
   * "some-node" would not include units from "sub-node-*":
   */
  for (int h = 0; h < topo->num_hosts; ++h) {
    dart_node_units_t * node_units = &topo->node_units[h];
    if (strncmp(node_units->host, hostname, DART_LOCALITY_HOST_MAX_SIZE)
        == 0) {
      *num_units        += node_units->num_units;
      *num_numa_domains += node_units->num_numa;
      *units             = node_units->units;
      host_found         = 1;
    }
  }
  if (!host_found) {
    DART_LOG_ERROR("dart__base__host_topology__module_units ! "
                   "no entry for host '%s')", hostname);
    return DART_ERR_NOTFOUND;
  }
  DART_LOG_TRACE("dart__base__host_topology__module_units > num_units: %d",
                 *num_units);
  return DART_OK;
}

