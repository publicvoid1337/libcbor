#include <cbor/common.h>
#include <cbor/data.h>
#include <cbor/maps.h>
#include <cbor/tags.h>
#include <stdio.h>
#include "cbor.h"

typedef struct {
  cbor_item_t* item;
  size_t index;
  int key_or_value;  // 0 for key, 1 for value
} cbor_parent_t;

typedef enum {
  PACKED_ERR_NONE,
  PACKED_ERR_UNDEFINED_REFERENCE,
  PACKED_ERR_OUT_OF_BOUNDS,
  PACKED_ERR_UNEXPECTED_FORMAT,
  PACKED_ERR_NOT_SUPPORTED
} packed_error_t;

packed_error_t _walk_and_replace(cbor_parent_t parent, cbor_item_t* item,
                                 cbor_item_t* current_packing_table,
                                 cbor_item_t** root);

packed_error_t _replace(cbor_parent_t parent, cbor_item_t* item,
                        cbor_item_t* new_item, cbor_item_t** root) {
  // Root replacement is handled by the walk algorithm itself.
  if (parent.item == NULL) {
    cbor_decref(&item);
    *root = new_item;
    return PACKED_ERR_NONE;
  }
  switch (cbor_typeof(parent.item)) {
    case CBOR_TYPE_ARRAY: {
      cbor_item_t* old_item = cbor_array_get(parent.item, parent.index);
      if (old_item != NULL) {
        cbor_decref(&old_item);
      }
      if (!cbor_array_replace(parent.item, parent.index, new_item)) {
        return PACKED_ERR_OUT_OF_BOUNDS;
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
      return PACKED_ERR_UNEXPECTED_FORMAT;
  }
  return PACKED_ERR_NONE;
}

/* Returns reference to packing table (cbor array) */
cbor_item_t* _consume_table_113(cbor_parent_t parent, cbor_item_t* item,
                                cbor_item_t** new_root) {
  cbor_item_t* arr = cbor_tag_item(item);
  assert(cbor_typeof(arr) == CBOR_TYPE_ARRAY);  // todo change to error handling
  cbor_item_t* packing_table = cbor_array_get(arr, 0);
  assert(cbor_typeof(packing_table) ==
         CBOR_TYPE_ARRAY);  // todo change to error handling
  cbor_item_t* _new_root = cbor_array_get(arr, 1);

  cbor_incref(packing_table);
  cbor_incref(_new_root);
  *new_root = _new_root;

  if (parent.item != NULL) {
    _replace(parent, item, _new_root, new_root);
  }

  return packing_table;
}

/* SHARED ITEM REFERENCE or ARGUMENT REFERENCE */
packed_error_t _handle_tag_6(cbor_parent_t parent, cbor_item_t* item,
                             cbor_item_t* current_packing_table,
                             cbor_item_t** root) {
  cbor_item_t* sub = cbor_tag_item(item);
  packed_error_t result = PACKED_ERR_NONE;

  switch (cbor_typeof(sub)) {
    case CBOR_TYPE_UINT: { /* SHARED ITEM REFERENCE */
      // TODO: properly calculate index
      size_t index = cbor_get_uint8(sub);
      if (current_packing_table == NULL ||
          index >= cbor_array_size(current_packing_table)) {
        result = PACKED_ERR_UNDEFINED_REFERENCE;
      } else {
        cbor_item_t* unpacked_item =
            cbor_array_get(current_packing_table, index);
        _replace(parent, item, unpacked_item, root);
        cbor_decref(&unpacked_item);
      }
      break;
    }
    case CBOR_TYPE_NEGINT: { /* SHARED ITEM REFERENCE */
      result = PACKED_ERR_NOT_SUPPORTED;
      break;
    }
    case CBOR_TYPE_ARRAY: { /* ARGUMENT REFERENCE */
      result = PACKED_ERR_NOT_SUPPORTED;
      break;
    }
    case CBOR_TYPE_TAG: { /* most likely nested */
      // TODO: for now we assume only other tags 6 can be nested
      puts("handle_tag_6: TAG");
      cbor_parent_t new_parent = {.item = item};
      cbor_item_t* nested_tag = cbor_tag_item(item);
      _walk_and_replace(new_parent, nested_tag, current_packing_table, root);
      cbor_decref(&nested_tag);
      _handle_tag_6(parent, item, current_packing_table, root);
      break;
    }
    default: {
      result = PACKED_ERR_UNEXPECTED_FORMAT;
    }
  }
  cbor_decref(&sub);
  return result;
}

// todo: increase max nesting depth
packed_error_t _walk_and_replace(cbor_parent_t parent, cbor_item_t* item,
                                 cbor_item_t* current_packing_table,
                                 cbor_item_t** root) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_ARRAY: {
      for (size_t i = 0; i < cbor_array_size(item); i++) {
        cbor_parent_t new_parent = {.item = item, .index = i};
        cbor_item_t* child = cbor_array_get(item, i);
        packed_error_t error =
            _walk_and_replace(new_parent, child, current_packing_table, root);
        cbor_decref(&child);
        if (error != PACKED_ERR_NONE) {
          return error;
        }
      }
      break;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(item);
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        cbor_parent_t new_parent = {
            .item = item, .index = i, .key_or_value = 0};
        _walk_and_replace(new_parent, pairs[i].key, current_packing_table,
                          root);
        new_parent.key_or_value = 1;
        _walk_and_replace(new_parent, pairs[i].value, current_packing_table,
                          root);
      }
      break;
    }
    case CBOR_TYPE_TAG:
      switch (cbor_tag_value(item)) {
        /* PACKING TABLE SETUP */
        case 113: {
          cbor_item_t* new_root;
          cbor_item_t* packing_table =
              _consume_table_113(parent, item, &new_root);
          if (parent.item == NULL) {
            cbor_decref(&item);
            item = new_root;
          }
          packed_error_t error =
              _walk_and_replace(parent, new_root, packing_table, root);
          cbor_decref(&packing_table);
          cbor_decref(&new_root);
          if (error != PACKED_ERR_NONE) {
            return error;
          }
          break;
        }
        case 128:
        case 129:
        case 130:
        case 131:
        case 132:
        case 133:
        case 134:
        case 135:
        case 136:
        case 137:
        case 138:
        case 139:
        case 140:
        case 141:
        case 142:
        case 143: {
          size_t tag_num = cbor_tag_value(item);
          size_t index;
          if (tag_num < 136) {
            index = tag_num - 128;
          } else {
            index = tag_num - 136;
          }
        }
        /* SHARED ITEM REFERENCE or ARGUMENT REFERENCE */
        case 6: {
          // TODO: catch error returned by function below
          _handle_tag_6(parent, item, current_packing_table, root);
          break;
        }
        /* Tag not specified by packed cbor */
        default: {
          cbor_parent_t new_parent = {.item = item};
          cbor_item_t* tag_child = cbor_tag_item(item);
          _walk_and_replace(new_parent, tag_child, current_packing_table, root);
          cbor_decref(&tag_child);
          break;
        }
      }
      break;
    default: {
      /* Normal leaf node - nothing to do */
      break;
    }
  }
  return PACKED_ERR_NONE;
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

unsigned char DATA2[] = {0x82, 0xD8, 0x71, 0x82, 0x81, 0xC6, 0x00, 0xD8, 0x71,
                         0x82, 0x81, 0x6C, 0x6E, 0x65, 0x73, 0x74, 0x65, 0x64,
                         0x20, 0x76, 0x61, 0x6C, 0x75, 0x65, 0xc6, 0x00, 0x00};

int main(void) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(DATA2, sizeof(DATA2), &res);
  assert(res.error.code == CBOR_ERR_NONE);
  cbor_describe(item, stdout);

  size_t serialized_size = cbor_serialized_size(item);
  printf("\n\nSerialized size: %zu bytes\n", serialized_size);

  puts("\n\n");
  cbor_item_t** root = &item;
  cbor_parent_t parent = {.item = NULL};

  packed_error_t error = _walk_and_replace(parent, item, NULL, root);
  if (error != PACKED_ERR_NONE) {
    fprintf(stderr, "Error occurred while walking and replacing items\n");
    return 1;
  }

  cbor_describe(*root, stdout);

  serialized_size = cbor_serialized_size(item);
  printf("\n\nSerialized size: %zu bytes\n", serialized_size);

  cbor_decref(&item);
  return 0;
}