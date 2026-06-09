// TODO: Fix imports
#include <cbor/data.h>
#include <stdint.h>
#include "cbor.h"

/* DEFINES */
#define PACKED_ENABLE_DEBUG 0
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

/* UTILITY FUNCTIONS */
packed_error_t _replace(parent_t parent, cbor_item_t* old_item,
                        cbor_item_t* new_item);

/* MAIN FUNCTION */
packed_response_t _neo_traverse(neo_rec_inf_t rec_inf);

cbor_item_t* cbor_unpack(cbor_item_t* target,
                         neo_tabledef_t* global_packing_table);