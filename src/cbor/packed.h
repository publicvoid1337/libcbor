// TODO: Fix imports
#include <cbor/data.h>
#include <stdint.h>
#include "cbor.h"

/* DEFINES */
#define PACKED_ENABLE_DEBUG 1
#define MAX_REFERENCE_DEPTH 5
#define MAX_ACTIVE_TABLES 5

/* TYPEDEFS */
typedef enum {
  PACKED_ERR_NONE,
  PACKED_ERR_UNDEFINED_REFERENCE,
  PACKED_ERR_OUT_OF_BOUNDS,
  PACKED_ERR_UNEXPECTED_FORMAT,
  PACKED_ERR_NOT_SUPPORTED,
  PACKED_ERR_MAX_REF_DEPTH_EXCEEDED,
  _NESTED,
  _RESOLVE_THEN_REPLACE,
  _TAG_1115,
  _CURR_REPLACED,
} packed_error_t;

typedef struct {
  cbor_item_t* item;
  size_t index;
  bool is_key;
} parent_t;

struct table_definition {
  bool is_basic;
  union {
    struct {
      cbor_item_t* combined_table;
    } basic;
    struct {
      cbor_item_t* shareditem_table;
      cbor_item_t* argument_table;
    } split;
  };
};

typedef struct {
  struct table_definition tables[MAX_ACTIVE_TABLES];
  uint8_t num_active;
  // size_t shareditem_idx;
  // size_t argument_idx;
} packing_ctx_t;

typedef struct {
  parent_t parent;
  cbor_item_t* item;
  packing_ctx_t packing_ctx;
  uint8_t ref_depth;
} recursion_info_t;

typedef struct neo_tabledef_s neo_tabledef_t;
typedef struct neo_tabledef_s {
  neo_tabledef_t* prev;
  bool is_basic;
  union {
    struct {
      cbor_item_t* combined_table;
    };
    struct {
      cbor_item_t* shareditem_table;
      cbor_item_t* argument_table;
    };
  } data;
} neo_tabledef_t;

typedef struct {
  cbor_item_t* curr;
  neo_tabledef_t* tabledef;
  uint8_t depth;
} neo_rec_inf_t;

typedef struct {
  packed_error_t err;
  struct {
    bool increase_depth : 1;
    bool new_tabledef : 1;
    bool replace_child : 1;
  } flags;
  struct {
    neo_tabledef_t new_table;
    cbor_item_t* new_child;
  } data;
} packed_response_t;

/* HELPER FUNCTIONS */
recursion_info_t _new_rec_info(packing_ctx_t new_packing_ctx,
                               cbor_item_t* new_item,
                               cbor_item_t* new_parent_item,
                               size_t new_parent_idx, bool new_parent_is_key,
                               uint8_t ref_depth);

const char* describe_error(packed_error_t error);

const char* describe_cbor_type(cbor_item_t* item);

void print_item_info(cbor_item_t* target, char* identifier);

#define PRINT_DEBUG_MSG(caller, item, parent) \
  do {                                        \
    printf("[%-14s]  ", caller);              \
    print_item_info(item, "curr");            \
    printf("  ");                             \
    print_item_info(parent, "parent");        \
    printf("\n");                             \
  } while (0)

#define CATCH_DECREF_RETURN_0(status)  \
  do {                                 \
    if ((status) != PACKED_ERR_NONE) { \
      return (status);                 \
    }                                  \
  } while (0)

#define CATCH_DECREF_RETURN_1(status, ...)                                 \
  do {                                                                     \
    packed_error_t _packed_status = (status);                              \
    if (_packed_status != PACKED_ERR_NONE) {                               \
      cbor_item_t* _items[] = {__VA_ARGS__};                               \
      for (size_t _i = 0; _i < sizeof(_items) / sizeof(_items[0]); _i++) { \
        if (_items[_i] != NULL) {                                          \
          cbor_decref(&_items[_i]);                                        \
        }                                                                  \
      }                                                                    \
      packed_response_t resp = {0};                                        \
      resp.err = _packed_status;                                           \
      return resp;                                                         \
    }                                                                      \
  } while (0)

#define _CATCH_DECREF_RETURN_GET_MACRO(_1, _2, NAME, ...) NAME

#define CATCH_DECREF_RETURN(...)                                     \
  _CATCH_DECREF_RETURN_GET_MACRO(__VA_ARGS__, CATCH_DECREF_RETURN_1, \
                                 CATCH_DECREF_RETURN_0)(__VA_ARGS__)

/* CBOR FUNCTIONS */
packed_error_t _concatenate(cbor_item_t* lhs, cbor_item_t* rhs,
                            cbor_item_t** out, cbor_type string_out_type);

packed_error_t _join(cbor_item_t* lhs, cbor_item_t* rhs, cbor_item_t** out);

packed_error_t _record(cbor_item_t* lhs, cbor_item_t* rhs, cbor_item_t** out);

/* UTILITY FUNCTIONS */

// bumps refcount of "out_shared_item" by 1
// if the user wishes to chase the returned reference,
// they should use the "out_..." parameters to update the local packing
// evironment
packed_error_t _packing_table_get(packing_ctx_t packing_ctx, size_t index,
                                  bool is_arg_ref,
                                  cbor_item_t** out_shared_item,
                                  cbor_item_t** out_table, size_t* out_index,
                                  uint8_t* out_num_active);

// if this returns PACKED_ERR_NONE old was mutated to new. Update ur
// references!
packed_error_t _replace(parent_t parent, cbor_item_t* old_item,
                        cbor_item_t* new_item);

packed_error_t _consume_table_definition(parent_t parent, cbor_item_t* item,
                                         cbor_item_t** new_root,
                                         cbor_item_t** new_table_1,
                                         cbor_item_t** new_table_2);

/* MAIN FUNCTION */
packed_response_t _neo_traverse(neo_rec_inf_t rec_inf);

cbor_item_t* cbor_unpack(cbor_item_t* target,
                         neo_tabledef_t* global_packing_table);