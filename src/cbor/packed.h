// TODO: Fix imports
#include "cbor.h"

/* DEFINES */
#define PACKED_ENABLE_DEBUG 1
#define MAX_REFERENCE_DEPTH 5

/* TYPEDEFS */
typedef enum {
  PACKED_ERR_NONE,
  PACKED_ERR_UNDEFINED_REFERENCE,
  PACKED_ERR_OUT_OF_BOUNDS,
  PACKED_ERR_UNEXPECTED_FORMAT,
  PACKED_ERR_NOT_SUPPORTED,
  PACKED_ERR_MAX_REF_DEPTH_EXCEEDED,
} packed_error_t;

typedef struct {
  cbor_item_t* item;
  size_t index;
  bool is_key;
} parent_t;

typedef struct {
  cbor_item_t* parent;
  size_t index;
  bool is_key;
} packing_ctx_t;

typedef struct tabledef_s tabledef_t;
typedef struct tabledef_s {
  tabledef_t* prev;
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
} tabledef_t;

typedef struct {
  cbor_item_t* curr;
  tabledef_t* tabledef;
  uint8_t depth;
} rec_inf_t;

typedef struct {
  packed_error_t err;
  struct {
    bool increase_depth : 1;
    bool new_tabledef : 1;
    bool replace_child : 1;
  } flags;
  struct {
    tabledef_t new_table;
    cbor_item_t* new_child;
  } data;
} packed_response_t;

/* HELPER FUNCTIONS */
const char* describe_error(packed_error_t error);

const char* describe_cbor_type(cbor_item_t* item);

void print_item_info(cbor_item_t* target, char* identifier);

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

packed_error_t _tabledef_get(tabledef_t* table, size_t idx, bool is_shareditem,
                             cbor_item_t** out);

/* UTILITY FUNCTIONS */

/* MAIN FUNCTION */
packed_response_t _traverse(rec_inf_t rec_inf);

packed_error_t _neo_replace(parent_t replace_ctx, cbor_item_t* old_item,
                            cbor_item_t* new_item);

#define _TRAVERSE(rec_inf, replace_ctx, old_item)                 \
  do {                                                            \
    tabledef_t* starting_table = rec_inf.tabledef;                \
    packed_response_t resp;                                       \
                                                                  \
    do {                                                          \
      resp = _traverse(rec_inf);                                  \
      if (resp.err != PACKED_ERR_NONE) {                          \
        break;                                                    \
      }                                                           \
                                                                  \
      if (resp.flags.replace_child) {                             \
        _neo_replace(replace_ctx, old_item, resp.data.new_child); \
        rec_inf.curr = resp.data.new_child;                       \
      }                                                           \
      if (resp.flags.new_tabledef) {                              \
        tabledef_t* new_tabledef = malloc(sizeof(tabledef_t));    \
        *new_tabledef = resp.data.new_table;                      \
        new_tabledef->prev = rec_inf.tabledef;                    \
        rec_inf.tabledef = new_tabledef;                          \
      }                                                           \
      if (resp.flags.increase_depth) {                            \
        rec_inf.depth++;                                          \
      }                                                           \
    } while (resp.flags.replace_child);                           \
                                                                  \
    while (rec_inf.tabledef != starting_table) {                  \
      cbor_decref(&rec_inf.tabledef->data.combined_table);        \
      tabledef_t* next = rec_inf.tabledef->prev;                  \
      free(rec_inf.tabledef);                                     \
      rec_inf.tabledef = next;                                    \
    }                                                             \
                                                                  \
    if (resp.err != PACKED_ERR_NONE) {                            \
      cbor_decref(&rec_inf.curr);                                 \
      return resp;                                                \
    }                                                             \
  } while (0)

cbor_item_t* cbor_unpack(cbor_item_t* target, tabledef_t* global_packing_table);