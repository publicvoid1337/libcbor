#include <cbor/arrays.h>
#include <cbor/cbor_export.h>
#include <cbor/common.h>
#include <cbor/data.h>
#include <cbor/ints.h>
#include <cbor/maps.h>
#include <cbor/serialization.h>
#include <cbor/tags.h>
#include <stdio.h>
#include "cbor.h"

typedef struct {
  cbor_item_t* item;
  size_t index;
  int key_or_value;  // 0 for key, 1 for value
} cbor_parent_t;

void _replace(cbor_parent_t parent, cbor_item_t* new_item) {
  if (parent.item == NULL) {
    return;
  }
  switch (cbor_typeof(parent.item)) {
    case CBOR_TYPE_ARRAY: {
      cbor_item_t* old_item = cbor_array_get(parent.item, parent.index);
      if (old_item != NULL) {
        cbor_decref(&old_item);
      }
      if (!cbor_array_replace(parent.item, parent.index, new_item)) {
        // index out of bounds
      }
      break;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(parent.item);
      cbor_item_t* old_item;
      if (parent.key_or_value == 0) {
        old_item = pairs[parent.index].key;
        pairs[parent.index].key = cbor_move(new_item);
      } else {
        old_item = pairs[parent.index].value;
        pairs[parent.index].value = cbor_move(new_item);
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
      cbor_tag_set_item(parent.item, cbor_move(new_item));
      break;
    }
    default:
      // should never happen
      break;
  }
}

/* Returns reference to packing table (cbor array) */
cbor_item_t* _consume_table_113(cbor_parent_t parent, cbor_item_t* item,
                                cbor_item_t** new_root) {
  cbor_item_t* arr = cbor_tag_item(item);
  assert(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  cbor_item_t* packing_table = cbor_array_get(arr, 0);
  assert(cbor_typeof(packing_table) == CBOR_TYPE_ARRAY);
  cbor_item_t* _new_root = cbor_array_get(arr, 1);

  /* Increment references since we're extracting items from the array */
  cbor_incref(packing_table);
  cbor_incref(_new_root);
  *new_root = _new_root;

  _replace(parent, _new_root);

  return packing_table;
}

// todo: increase max nesting depth
static void _walk_and_replace(cbor_parent_t parent, cbor_item_t* item,
                              cbor_item_t* current_packing_table) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_UINT:
    case CBOR_TYPE_NEGINT:
    case CBOR_TYPE_STRING:
    case CBOR_TYPE_BYTESTRING:
    case CBOR_TYPE_FLOAT_CTRL:
      break;
    /* Call recursively */
    case CBOR_TYPE_ARRAY:
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        cbor_parent_t new_parent = {.item = item, .index = i};
        _walk_and_replace(new_parent, cbor_array_get(item, i),
                          current_packing_table);
      }
      break;
    case CBOR_TYPE_MAP:
      struct cbor_pair* pairs = cbor_map_handle(item);
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        cbor_parent_t new_parent = {
            .item = item, .index = i, .key_or_value = 0};
        _walk_and_replace(new_parent, pairs[i].key, current_packing_table);
        new_parent.key_or_value = 1;
        _walk_and_replace(new_parent, pairs[i].value, current_packing_table);
      }

      break;
    case CBOR_TYPE_TAG:
      if (cbor_tag_value(item) == 113) {
        cbor_item_t* new_root;
        cbor_item_t* packing_table =
            _consume_table_113(parent, item, &new_root);
        _walk_and_replace(parent, new_root, packing_table);
      } else if (cbor_tag_value(item) == 6) {
        size_t index = cbor_get_uint64(cbor_tag_item(item));
        if (current_packing_table != NULL &&
            index < cbor_array_size(current_packing_table)) {
          cbor_item_t* new_item = cbor_array_get(current_packing_table, index);
          cbor_incref(new_item);
          _replace(parent, new_item);
        }
      }
      break;
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

int main(void) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(DATA, sizeof(DATA), &res);
  assert(res.error.code == CBOR_ERR_NONE);
  cbor_describe(item, stdout);

  size_t serialized_size = cbor_serialized_size(item);
  printf("\n\nSerialized size: %zu bytes\n", serialized_size);

  puts("\n\n");

  cbor_parent_t parent = {.item = NULL};
  _walk_and_replace(parent, item, NULL);

  // Post processing: if the root is a packing table, replace it with the new
  // root might want to do this differently in the future
  if (cbor_typeof(item) == CBOR_TYPE_TAG && cbor_tag_value(item) == 113) {
    cbor_item_t* arr = cbor_tag_item(item);
    if (cbor_typeof(arr) == CBOR_TYPE_ARRAY && cbor_array_size(arr) >= 2) {
      cbor_item_t* new_root = cbor_array_get(arr, 1);
      cbor_incref(new_root);
      cbor_decref(&item);
      item = new_root;
    }
  }

  cbor_describe(item, stdout);

  serialized_size = cbor_serialized_size(item);
  printf("\n\nSerialized size: %zu bytes\n", serialized_size);

  cbor_decref(&item);
  return 0;
}