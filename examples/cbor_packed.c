#include <cbor/common.h>
#include <cbor/data.h>
#include <cbor/maps.h>
#include <cbor/tags.h>
#include <stdio.h>
#include "cbor.h"

#define ENABLE_DEBUG 1

typedef enum {
  PACKED_ERR_NONE,
  PACKED_ERR_UNDEFINED_REFERENCE,
  PACKED_ERR_OUT_OF_BOUNDS,
  PACKED_ERR_UNEXPECTED_FORMAT,
  PACKED_ERR_NOT_SUPPORTED,
  _NESTED,
  _RESOLVE_THEN_REPLACE
} packed_error_t;

typedef struct {
  cbor_item_t* item;
  size_t index;
  bool is_key;
} parent_t;

typedef struct callback {
  cbor_item_t* new_item;
  cbor_item_t* new_packing_table;
} callback_t;

typedef struct {
  packed_error_t error;  // Should be something with more semantics
  callback_t callback;
} response_t;

typedef struct {
  parent_t parent;
  cbor_item_t* item;
  cbor_item_t* current_packing_table;  // should me multiple in the future
} recursion_info_t;

#define HANDLE_CALLBACK(rec_inf, callback)                        \
  do {                                                            \
    if (callback.new_item != NULL) {                              \
      rec_inf.item = callback.new_item;                           \
    }                                                             \
    if (callback.new_packing_table != NULL) {                     \
      rec_inf.current_packing_table = callback.new_packing_table; \
    }                                                             \
  } while (0)

response_t _new_response(packed_error_t error, cbor_item_t* cb_new_item,
                         cbor_item_t* cb_new_packing_table) {
  callback_t cb = {.new_item = cb_new_item,
                   .new_packing_table = cb_new_packing_table};
  response_t resp = {.error = error, .callback = cb};
  return resp;
}

recursion_info_t _new_rec_info(cbor_item_t* new_packing_table,
                               cbor_item_t* new_item,
                               cbor_item_t* new_parent_item,
                               size_t new_parent_idx, bool new_parent_is_key) {
  parent_t parent = {.index = new_parent_idx,
                     .item = new_parent_item,
                     .is_key = new_parent_is_key};
  recursion_info_t rec_inf = {.parent = parent,
                              .current_packing_table = new_packing_table,
                              .item = new_item};
  return rec_inf;
}

char* describe_error(packed_error_t error) {
  switch (error) {
    case PACKED_ERR_NONE:
      return "No error";
    case PACKED_ERR_UNDEFINED_REFERENCE:
      return "Undefined reference in packing table";
    case PACKED_ERR_OUT_OF_BOUNDS:
      return "Index out of bounds in packing table";
    case PACKED_ERR_UNEXPECTED_FORMAT:
      return "Unexpected format in packed CBOR data";
    case PACKED_ERR_NOT_SUPPORTED:
      return "Packed CBOR feature not supported";
    case _NESTED:
      return "Nested structure requires additional handling";
    default:
      return "Unknown error";
  }
}

char* describe_cbor_type(cbor_item_t* item) {
  const cbor_type type = cbor_typeof(item);
  switch (type) {
    case CBOR_TYPE_UINT:
      return "UINT";
    case CBOR_TYPE_NEGINT:
      return "INT";
    case CBOR_TYPE_BYTESTRING:
      return "BYTESTR";
    case CBOR_TYPE_STRING:
      return "TEXTSTR";
    case CBOR_TYPE_ARRAY:
      return "ARR";
    case CBOR_TYPE_MAP:
      return "MAP";
    case CBOR_TYPE_TAG:
      return "TAG";
    case CBOR_TYPE_FLOAT_CTRL:
      return "FLOAT/CTRL";
    default:
      return "TYPE_UNKNOWN";
  }
}

void print_item_info(cbor_item_t* target, char* identifier) {
  if (target == NULL) {
    printf("%s=NULL", identifier);
    return;
  }
  switch (cbor_typeof(target)) {
    case CBOR_TYPE_ARRAY: {
      size_t size = cbor_array_size(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_BYTESTRING: {
      size_t size = cbor_bytestring_length(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_FLOAT_CTRL: {
      size_t value = cbor_float_get_float(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_MAP: {
      size_t size = cbor_map_size(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_NEGINT: {
      int value = cbor_get_int(target);
      printf("%s=%s(%d)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_UINT: {
      size_t value = cbor_get_uint64(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_STRING: {
      size_t size = cbor_string_length(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_TAG: {
      size_t value = cbor_tag_value(target);
      printf("%s=%s(%ld)", identifier, describe_cbor_type(target), value);
      return;
    }
  }
}

#define PRINT_DEBUG_MSG(caller, item, parent) \
  do {                                        \
    printf("[%-14s]  ", caller);              \
    print_item_info(item, "curr");            \
    printf(" ");                              \
    print_item_info(parent, "parent");        \
    printf("\n");                             \
  } while (0)

response_t _replace_N(parent_t parent, cbor_item_t* item,
                      cbor_item_t* new_item) {
#if ENABLE_DEBUG
  PRINT_DEBUG_MSG("replace", item, parent.item);
#endif

  if (parent.item == NULL) {
    cbor_decref(&item);
    return _new_response(PACKED_ERR_NONE, new_item, NULL);
  }
  switch (cbor_typeof(parent.item)) {
    case CBOR_TYPE_ARRAY: {
      cbor_item_t* old_item = cbor_array_get(parent.item, parent.index);
      if (!cbor_array_replace(parent.item, parent.index, new_item)) {
        if (old_item != NULL) {
          cbor_decref(&old_item);
        }
        return _new_response(PACKED_ERR_OUT_OF_BOUNDS, NULL, NULL);
      }
      if (old_item != NULL) {
        cbor_decref(&old_item);
      }
      break;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(parent.item);
      cbor_item_t* old_item;
      cbor_incref(new_item);
      if (parent.is_key) {
        old_item = pairs[parent.index].key;
        pairs[parent.index].key = new_item;
      } else {
        old_item = pairs[parent.index].value;
        pairs[parent.index].value = new_item;
      }
      if (old_item != NULL) {
        cbor_decref(&old_item);
      }
      break;
    }
    case CBOR_TYPE_TAG: {
      cbor_item_t* old_item = cbor_tag_item(parent.item);
      if (old_item != NULL) {
        cbor_decref(&old_item);
      }
      cbor_tag_set_item(parent.item, new_item);
      break;
    }
    default:
      return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }
  return _new_response(PACKED_ERR_NONE, new_item, NULL);
}

response_t _handle_tag_113(parent_t parent, cbor_item_t* item) {
#if ENABLE_DEBUG
  PRINT_DEBUG_MSG("handle_tag_113", item, parent.item);
#endif

  cbor_item_t* arr = cbor_tag_item(item);
  if (arr == NULL) {
    return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }
  if (cbor_typeof(arr) != CBOR_TYPE_ARRAY) {
    return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }

  cbor_item_t* packing_table = cbor_array_get(arr, 0);
  if (packing_table == NULL) {
    return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }
  if (cbor_typeof(packing_table) != CBOR_TYPE_ARRAY) {
    return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }

  cbor_item_t* packed_data = cbor_array_get(arr, 1);
  if (packed_data == NULL) {
    return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }

  response_t resp = _replace_N(parent, item, packed_data);
  if (resp.error != PACKED_ERR_NONE) {
    return resp;
  }

  cbor_decref(&arr);
  // cbor_decref(&packed_data);

  return _new_response(PACKED_ERR_NONE, resp.callback.new_item, packing_table);
}

response_t _handle_tag_6(cbor_item_t* packing_table, parent_t parent,
                         cbor_item_t* item) {
#if ENABLE_DEBUG
  PRINT_DEBUG_MSG("handle_tag_6", item, parent.item);
#endif

  cbor_item_t* tag_item = cbor_tag_item(item);
  if (tag_item == NULL) {
    return _new_response(PACKED_ERR_UNEXPECTED_FORMAT, NULL, NULL);
  }

  if (cbor_typeof(tag_item) == CBOR_TYPE_UINT) {
    size_t index = cbor_get_uint8(tag_item);
    if (packing_table == NULL || index >= cbor_array_size(packing_table)) {
      cbor_decref(&tag_item);
      return _new_response(PACKED_ERR_UNDEFINED_REFERENCE, NULL, NULL);
    }

    cbor_item_t* unpacked_item = cbor_array_get(packing_table, index);

    // staging
    if (cbor_typeof(unpacked_item) == CBOR_TYPE_TAG &&
        cbor_tag_value(unpacked_item) == 6) {
      cbor_decref(&tag_item);
      cbor_decref(&unpacked_item);
      return _new_response(_RESOLVE_THEN_REPLACE, NULL, NULL);
    }

    response_t resp = _replace_N(parent, item, unpacked_item);
    if (resp.error != PACKED_ERR_NONE) {
      cbor_decref(&unpacked_item);
      cbor_decref(&tag_item);
      return resp;
    }
    // HANDLE_CALLBACK(rec_inf, resp.callback);

    if (parent.item != NULL) {
      cbor_decref(&unpacked_item);
    }
    cbor_decref(&tag_item);
    return resp;
  }
  cbor_decref(&tag_item);

  return _new_response(_NESTED, NULL, NULL);
}

response_t _traverse(recursion_info_t rec_inf) {
#if ENABLE_DEBUG
  PRINT_DEBUG_MSG("traverse", rec_inf.item, rec_inf.parent.item);
#endif

  switch (cbor_typeof(rec_inf.item)) {
    case CBOR_TYPE_ARRAY: {
      for (size_t i = 0; i < cbor_array_size(rec_inf.item); i++) {
        cbor_item_t* child = cbor_array_get(rec_inf.item, i);
        response_t resp = _traverse(_new_rec_info(
            rec_inf.current_packing_table, child, rec_inf.item, i, false));
        if (resp.error != PACKED_ERR_NONE) {
          return resp;
        }
        HANDLE_CALLBACK(rec_inf, resp.callback);
        cbor_decref(&child);
      }
      return _new_response(PACKED_ERR_NONE, NULL, NULL);
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(rec_inf.item);
      for (size_t i = 0; i < cbor_map_size(rec_inf.item); i++) {
        response_t resp;
        cbor_item_t* key = cbor_incref(pairs[i].key);
        resp = _traverse(_new_rec_info(rec_inf.current_packing_table, key,
                                       rec_inf.item, i, true));
        if (resp.error != PACKED_ERR_NONE) {
          cbor_decref(&key);
          return resp;
        }
        HANDLE_CALLBACK(rec_inf, resp.callback);
        cbor_decref(&key);

        cbor_item_t* value = cbor_incref(pairs[i].value);
        resp = _traverse(_new_rec_info(rec_inf.current_packing_table, value,
                                       rec_inf.item, i, false));
        if (resp.error != PACKED_ERR_NONE) {
          cbor_decref(&value);
          return resp;
        }
        HANDLE_CALLBACK(rec_inf, resp.callback);
        cbor_decref(&value);
      }
      return _new_response(PACKED_ERR_NONE, NULL, NULL);
    }
    case CBOR_TYPE_TAG: {
      switch (cbor_tag_value(rec_inf.item)) {
        case 113: {
          response_t resp = _handle_tag_113(rec_inf.parent, rec_inf.item);
          if (resp.error != PACKED_ERR_NONE) {
            return resp;
          }
          HANDLE_CALLBACK(rec_inf, resp.callback);

          resp = _traverse(_new_rec_info(
              rec_inf.current_packing_table, rec_inf.item, rec_inf.parent.item,
              rec_inf.parent.index, rec_inf.parent.is_key));
          if (resp.error != PACKED_ERR_NONE) {
            if (rec_inf.current_packing_table != NULL) {
              cbor_decref(&rec_inf.current_packing_table);
            }
            return _new_response(resp.error, rec_inf.item, NULL);
          }
          HANDLE_CALLBACK(rec_inf, resp.callback);

          if (rec_inf.current_packing_table != NULL) {
            cbor_decref(&rec_inf.current_packing_table);
          }

          if (rec_inf.parent.item != NULL) {
            return _new_response(PACKED_ERR_NONE, NULL, NULL);
          }
          return _new_response(PACKED_ERR_NONE, rec_inf.item, NULL);
        }
        case 6: {
          response_t resp = _handle_tag_6(rec_inf.current_packing_table,
                                          rec_inf.parent, rec_inf.item);
          if (resp.error == _NESTED) {
            cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
            resp = _traverse(_new_rec_info(rec_inf.current_packing_table,
                                           tag_child, rec_inf.item, 0, false));
            cbor_decref(&tag_child);
            if (resp.error != PACKED_ERR_NONE) {
              return resp;
            }

            resp = _handle_tag_6(rec_inf.current_packing_table, rec_inf.parent,
                                 rec_inf.item);
            if (resp.error == _RESOLVE_THEN_REPLACE) {
              resp = _traverse(rec_inf);  // restart process
              return resp;
            }
            if (resp.error != PACKED_ERR_NONE) {
              return resp;
            }
            HANDLE_CALLBACK(rec_inf, resp.callback);

            return _new_response(PACKED_ERR_NONE, rec_inf.item, NULL);
          }
          if (resp.error == _RESOLVE_THEN_REPLACE) {
            cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
            size_t index = cbor_get_uint8(tag_child);
            cbor_item_t* unpacked_item =
                cbor_array_get(rec_inf.current_packing_table, index);
            resp = _traverse(
                _new_rec_info(rec_inf.current_packing_table, unpacked_item,
                              rec_inf.current_packing_table, index, false));
            if (resp.error != PACKED_ERR_NONE) {
              return resp;
            }
            resp = _handle_tag_6(rec_inf.current_packing_table, rec_inf.parent,
                                 rec_inf.item);
            if (resp.error != PACKED_ERR_NONE) {
              return resp;
            }
            HANDLE_CALLBACK(rec_inf, resp.callback);

            cbor_decref(&tag_child);
            cbor_decref(&unpacked_item);
          }
          if (resp.error == PACKED_ERR_NONE) {
            HANDLE_CALLBACK(rec_inf, resp.callback);
            // staging
            if (rec_inf.parent.item == NULL) {
              return _new_response(PACKED_ERR_NONE, rec_inf.item, NULL);
            }
            return _new_response(PACKED_ERR_NONE, NULL, NULL);
          }
          return resp;
        }
        default: {
          /* Tag not specified by packed cbor */
          cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
          response_t resp =
              _traverse(_new_rec_info(rec_inf.current_packing_table, tag_child,
                                      rec_inf.item, 0, false));
          cbor_decref(&tag_child);
          if (resp.error != PACKED_ERR_NONE) {
            return resp;
          }
          HANDLE_CALLBACK(rec_inf, resp.callback);

          return _new_response(PACKED_ERR_NONE, NULL, NULL);
        }
      }
    }
    default: {
      /* normal leaf node */
      return _new_response(PACKED_ERR_NONE, NULL, NULL);
    }
  }
}

//
//
//

unsigned char DATA[] = {
    0xD8, 0x71, 0x82, 0x87, 0x65, 0x70, 0x72, 0x69, 0x63, 0x65, 0x68, 0x63,
    0x61, 0x74, 0x65, 0x67, 0x6F, 0x72, 0x79, 0x66, 0x61, 0x75, 0x74, 0x68,
    0x6F, 0x72, 0x65, 0x74, 0x69, 0x74, 0x6C, 0x65, 0x67, 0x66, 0x69, 0x63,
    0x74, 0x69, 0x6F, 0x6E, 0xFB, 0x40, 0x21, 0xE6, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x64, 0x69, 0x73, 0x62, 0x6A, 0xA1, 0x65, 0x73, 0x74, 0x6F, 0x72,
    0x65, 0xA2, 0x64, 0x62, 0x6F, 0x6F, 0x6B, 0x84, 0xA4, 0xC6, 0x01, 0x69,
    0x72, 0x65, 0x66, 0x65, 0x72, 0x65, 0x6E, 0x63, 0x65, 0xC6, 0x02, 0x6A,
    0x4E, 0x69, 0x67, 0x65, 0x6C, 0x20, 0x52, 0x65, 0x65, 0x73, 0xC6, 0x03,
    0x76, 0x53, 0x61, 0x79, 0x69, 0x6E, 0x67, 0x73, 0x20, 0x6F, 0x66, 0x20,
    0x74, 0x68, 0x65, 0x20, 0x43, 0x65, 0x6E, 0x74, 0x75, 0x72, 0x79, 0xC6,
    0x00, 0xC6, 0x05, 0xA4, 0xC6, 0x01, 0xC6, 0x04, 0xC6, 0x02, 0x6C, 0x45,
    0x76, 0x65, 0x6C, 0x79, 0x6E, 0x20, 0x57, 0x61, 0x75, 0x67, 0x68, 0xC6,
    0x03, 0x6F, 0x53, 0x77, 0x6F, 0x72, 0x64, 0x20, 0x6F, 0x66, 0x20, 0x48,
    0x6F, 0x6E, 0x6F, 0x75, 0x72, 0xC6, 0x00, 0xFB, 0x40, 0x29, 0xFA, 0xE1,
    0x47, 0xAE, 0x14, 0x7B, 0xA5, 0xC6, 0x01, 0xC6, 0x04, 0xC6, 0x02, 0x6F,
    0x48, 0x65, 0x72, 0x6D, 0x61, 0x6E, 0x20, 0x4D, 0x65, 0x6C, 0x76, 0x69,
    0x6C, 0x6C, 0x65, 0xC6, 0x03, 0x69, 0x4D, 0x6F, 0x62, 0x79, 0x20, 0x44,
    0x69, 0x63, 0x6B, 0xC6, 0x06, 0x6D, 0x30, 0x2D, 0x35, 0x35, 0x33, 0x2D,
    0x32, 0x31, 0x33, 0x31, 0x31, 0x2D, 0x33, 0xC6, 0x00, 0xC6, 0x05, 0xA5,
    0xC6, 0x01, 0xC6, 0x04, 0xC6, 0x02, 0x70, 0x4A, 0x2E, 0x20, 0x52, 0x2E,
    0x20, 0x52, 0x2E, 0x20, 0x54, 0x6F, 0x6C, 0x6B, 0x69, 0x65, 0x6E, 0xC6,
    0x03, 0x75, 0x54, 0x68, 0x65, 0x20, 0x4C, 0x6F, 0x72, 0x64, 0x20, 0x6F,
    0x66, 0x20, 0x74, 0x68, 0x65, 0x20, 0x52, 0x69, 0x6E, 0x67, 0x73, 0xC6,
    0x06, 0x6D, 0x30, 0x2D, 0x33, 0x39, 0x35, 0x2D, 0x31, 0x39, 0x33, 0x39,
    0x35, 0x2D, 0x38, 0xC6, 0x00, 0xFB, 0x40, 0x36, 0xFD, 0x70, 0xA3, 0xD7,
    0x0A, 0x3D, 0x67, 0x62, 0x69, 0x63, 0x79, 0x63, 0x6C, 0x65, 0xA2, 0x65,
    0x63, 0x6F, 0x6C, 0x6F, 0x72, 0x63, 0x72, 0x65, 0x64, 0xC6, 0x00, 0xFB,
    0x40, 0x33, 0xF3, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33};

unsigned char DATA2[] = {0xD8, 0x71, 0x82, 0x81, 0x00, 0xC6, 0x00};

unsigned char DATA3[] = {0xD8, 0x71, 0x82, 0x82, 0xC6, 0x01, 0x00, 0xC6, 0x00};

unsigned char DATA4[] = {0xD8, 0x71, 0x82, 0x84, 0xC6, 0xC6, 0x01, 0x02,
                         0x63, 0x24, 0x24, 0x24, 0x00, 0xC6, 0xC6, 0x03};

int main(void) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(DATA4, sizeof(DATA4), &res);
  assert(res.error.code == CBOR_ERR_NONE);

  puts("\n");
  cbor_describe(item, stdout);
  size_t serialized_size = cbor_serialized_size(item);
  printf("  ---->  Serialized size: %zu bytes\n\n", serialized_size);

  recursion_info_t rec_inf = _new_rec_info(NULL, item, NULL, 0, false);
  response_t resp = _traverse(rec_inf);
  HANDLE_CALLBACK(rec_inf, resp.callback);
  if (resp.error != PACKED_ERR_NONE) {
    printf("\nCRASHED: %s\n", describe_error(resp.error));
  }

  puts("");
  cbor_describe(rec_inf.item, stdout);
  serialized_size = cbor_serialized_size(rec_inf.item);
  printf("  ---->  Serialized size: %zu bytes\n", serialized_size);

  cbor_decref(&rec_inf.item);
  if (rec_inf.current_packing_table != NULL) {
    cbor_decref(&rec_inf.current_packing_table);
  }
  return 0;
}