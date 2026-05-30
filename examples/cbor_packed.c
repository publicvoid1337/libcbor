#include "../src/cbor/packed.h"

unsigned char SIMPLE_normal_replace[] = {0xD8, 0x71, 0x82, 0x81,
                                         0x18, 0x43, 0xE0};

unsigned char SIMPLE_undef_ref[] = {0xD8, 0x71, 0x82, 0x81, 0x18, 0x43, 0xE1};

unsigned char SIMPLE_nested_ref[] = {0xD8, 0x71, 0x82, 0x82,
                                     0x18, 0x43, 0xE0, 0xE1};

unsigned char SIMPLE_loop[] = {0xD8, 0x71, 0x82, 0x82, 0xE1, 0xE0, 0xE1};

unsigned char SIMPLE_nested_table_1[] = {0xD8, 0x71, 0x82, 0x81, 0xD8, 0x71,
                                         0x82, 0x81, 0x18, 0x43, 0xE0, 0xE0};

unsigned char SIMPLE_nested_table_2[] = {0xD8, 0x71, 0x82, 0x81, 0x18, 0x43,
                                         0xD8, 0x71, 0x82, 0x81, 0xE1, 0xE0};

unsigned char ARG_REF_CONCAT[] = {
    0xD8, 0x71, 0x82, 0x83, 0x66, 0x66, 0x6F, 0x6F, 0x62, 0x61,
    0x72, 0x44, 0x66, 0x6F, 0x6F, 0x62, 0x62, 0x66, 0x6F, 0x83,
    0xD8, 0x80, 0x61, 0x74, 0xD8, 0x81, 0x63, 0x61, 0x72, 0x74,
    0xD8, 0x82, 0x65, 0x6F, 0x62, 0x61, 0x72, 0x74};

int main(void) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(ARG_REF_CONCAT, sizeof(ARG_REF_CONCAT), &res);
  assert(res.error.code == CBOR_ERR_NONE);

  puts("\n");
  cbor_describe(item, stdout);
  size_t serialized_size = cbor_serialized_size(item);
  printf("  ---->  Serialized size: %zu bytes\n\n", serialized_size);

  recursion_info_t rec_inf = _new_rec_info(NULL, item, NULL, 0, false, 0, 0);
  cbor_item_t* new_item = NULL;
  packed_error_t ret = _traverse(rec_inf, &new_item);
  if (new_item != NULL) {
    rec_inf.item = new_item;
  }
  if (ret != PACKED_ERR_NONE) {
    printf("\nCRASHED: %s\n", describe_error(ret));
  } else {
    puts("");
    cbor_describe(rec_inf.item, stdout);
    serialized_size = cbor_serialized_size(rec_inf.item);
    printf("  ---->  Serialized size: %zu bytes\n", serialized_size);
  }

  cbor_decref(&rec_inf.item);
  if (rec_inf.num_active > 0) {
    cbor_decref(&rec_inf.tables[rec_inf.num_active - 1]);
    rec_inf.num_active--;
  }
  return 0;
}
