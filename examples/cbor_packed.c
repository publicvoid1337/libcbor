#include <cbor/data.h>
#include <cbor/maps.h>
#include <stdio.h>
#include "cbor.h"

int main(void) {
  puts("Hello");
  return 0;
}

typedef struct {
  cbor_item_t* item;
  size_t index;
  int key_or_value;  // 0 for key, 1 for value
} cbor_parent_t;

static void _consume_table_113(cbor_parent_t parent, cbor_item_t* item) {}

static void _walk_and_replace(cbor_parent_t parent, cbor_item_t* item) {
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
        _walk_and_replace(new_parent, cbor_array_get(item, i));
      }
      break;
    case CBOR_TYPE_MAP:
      struct cbor_pair* pairs = cbor_map_handle(item);
      for (size_t i = 0; i < cbor_map_size(item); i++) {
        cbor_parent_t new_parent = {
            .item = item, .index = i, .key_or_value = 0};
        _walk_and_replace(new_parent, pairs[i].key);
        new_parent.key_or_value = 1;
        _walk_and_replace(new_parent, pairs[i].value);
      }

      break;
    case CBOR_TYPE_TAG:
      if (cbor_tag_value(item) == 113) {
        _consume_table_113(parent, item);
      }
      break;
  }
}