// TODO: Fix imports
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
  _RESOLVE_THEN_REPLACE
} packed_error_t;

typedef struct {
  cbor_item_t* item;
  size_t index;
  bool is_key;
} parent_t;

typedef struct {
  parent_t parent;
  cbor_item_t* item;
  cbor_item_t* tables[MAX_ACTIVE_TABLES];
  uint8_t num_active;
  uint8_t ref_depth;
} recursion_info_t;

/* HELPER FUNCTIONS */
recursion_info_t _new_rec_info(
    cbor_item_t* new_packing_tables[MAX_ACTIVE_TABLES], cbor_item_t* new_item,
    cbor_item_t* new_parent_item, size_t new_parent_idx, bool new_parent_is_key,
    uint8_t ref_depth, uint8_t num_active);

const char* describe_error(packed_error_t error);

const char* describe_cbor_type(cbor_item_t* item);

void print_item_info(cbor_item_t* target, char* identifier);

#define PRINT_DEBUG_MSG(caller, item, parent) \
  do {                                        \
    printf("[%-14s]  ", caller);              \
    print_item_info(item, "curr");            \
    printf(" ");                              \
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
      return _packed_status;                                               \
    }                                                                      \
  } while (0)

#define _CATCH_DECREF_RETURN_GET_MACRO(_1, _2, NAME, ...) NAME

#define CATCH_DECREF_RETURN(...)                                     \
  _CATCH_DECREF_RETURN_GET_MACRO(__VA_ARGS__, CATCH_DECREF_RETURN_1, \
                                 CATCH_DECREF_RETURN_0)(__VA_ARGS__)

/* CBOR FUNCTIONS */
packed_error_t _concatenate(cbor_item_t* lhs, cbor_item_t* rhs,
                            cbor_item_t** out);

packed_error_t _join(cbor_item_t* lhs, cbor_item_t* rhs, cbor_item_t** out,
                     bool inverted);

/* UTILITY FUNCTIONS */

// bumps refcount of "out_shared_item" by 1
// if the user wishes to chase the returned reference,
// they should use the "out_..." parameters to update the local packing
// evironment
packed_error_t _packing_table_get(cbor_item_t* tables[MAX_ACTIVE_TABLES],
                                  uint8_t num_active, size_t index,
                                  cbor_item_t** out_shared_item,
                                  cbor_item_t** out_table, size_t* out_index,
                                  uint8_t* out_num_active);

// if this returns PACKED_ERR_NONE old was mutated to new. Update ur
// references!
packed_error_t _replace(parent_t parent, cbor_item_t* old_item,
                        cbor_item_t* new_item);

/* HANDLERS */
packed_error_t _handle_tag_6(cbor_item_t* packing_tables[MAX_ACTIVE_TABLES],
                             uint8_t num_active, parent_t parent,
                             cbor_item_t* item, cbor_item_t** new_item);

packed_error_t _handle_tag_113(parent_t parent, cbor_item_t* item,
                               cbor_item_t** new_root, cbor_item_t** new_table);

/* MAIN FUNCTION */
packed_error_t _traverse(recursion_info_t rec_inf, cbor_item_t** new_item);