#include "../src/cbor/packed.h"

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

unsigned char DATA5[] = {0xD8, 0x71, 0x82, 0x82, 0xC6, 0xC6, 0x01,
                         0x63, 0x24, 0x24, 0x24, 0xC6, 0x00};

unsigned char DATA6[] = {0xD8, 0x71, 0x82, 0x82, 0xC6,
                         0x01, 0xC6, 0x00, 0xC6, 0x00};

int main(void) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(DATA4, sizeof(DATA4), &res);
  assert(res.error.code == CBOR_ERR_NONE);

  puts("\n");
  cbor_describe(item, stdout);
  size_t serialized_size = cbor_serialized_size(item);
  printf("  ---->  Serialized size: %zu bytes\n\n", serialized_size);

  recursion_info_t rec_inf = _new_rec_info(NULL, item, NULL, 0, false, 0, 0);
  cbor_item_t* new_item = NULL;
  cbor_item_t* new_packing_table = NULL;
  packed_error_t ret = _traverse(rec_inf, &new_item, &new_packing_table);
  HANDLE_CALLBACK(rec_inf, new_item, new_packing_table);
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

  /*
  // CONCAT TEST 1
  {
    puts("\nCONCAT TEST - ARRAY");
    cbor_item_t* lhs = cbor_new_definite_array(2);
    cbor_array_push(lhs, cbor_move(cbor_build_uint8(1)));
    cbor_array_push(lhs, cbor_move(cbor_build_uint8(2)));
    cbor_item_t* rhs = cbor_new_definite_array(2);
    cbor_array_push(rhs, cbor_move(cbor_build_uint8(3)));
    cbor_array_push(rhs, cbor_move(cbor_build_uint8(4)));

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 1

  // CONCAT TEST 2
  {
    puts("\nCONCAT TEST - MAPS:normal");
    cbor_item_t* lhs = cbor_new_definite_map(2);
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(10)),
                                    .value = cbor_move(cbor_build_uint8(1))});
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(20)),
                                    .value = cbor_move(cbor_build_uint8(2))});
    cbor_item_t* rhs = cbor_new_definite_map(2);
    cbor_map_add(rhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(30)),
                                    .value = cbor_move(cbor_build_uint8(3))});
    cbor_map_add(rhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(40)),
                                    .value = cbor_move(cbor_build_uint8(4))});

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 2

  // CONCAT TEST 3
  {
    puts("\nCONCAT TEST - MAPS:overlapping keys");
    cbor_item_t* lhs = cbor_new_definite_map(2);
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(10)),
                                    .value = cbor_move(cbor_build_uint8(1))});
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(20)),
                                    .value = cbor_move(cbor_build_uint8(2))});
    cbor_item_t* rhs = cbor_new_definite_map(2);
    cbor_map_add(rhs, (struct cbor_pair){
                          .key = cbor_move(cbor_build_uint8(10)),
                          .value = cbor_move(cbor_build_string("replaced"))});
    cbor_map_add(rhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(40)),
                                    .value = cbor_move(cbor_build_uint8(4))});

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 3

  // CONCAT TEST 4
  {
    puts("\nCONCAT TEST - MAPS:undef in lhs");
    cbor_item_t* lhs = cbor_new_definite_map(2);
    cbor_map_add(lhs,
                 (struct cbor_pair){
                     .key = cbor_move(cbor_build_uint8(10)),
                     .value = cbor_move(cbor_build_ctrl(CBOR_CTRL_UNDEF))});
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(20)),
                                    .value = cbor_move(cbor_build_uint8(2))});
    cbor_item_t* rhs = cbor_new_definite_map(2);
    cbor_map_add(rhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(30)),
                                    .value = cbor_move(cbor_build_uint8(3))});
    cbor_map_add(rhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(40)),
                                    .value = cbor_move(cbor_build_uint8(4))});

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 4

  // CONCAT TEST 5
  {
    puts("\nCONCAT TEST - MAPS:undef in rhs");
    cbor_item_t* lhs = cbor_new_definite_map(2);
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(10)),
                                    .value = cbor_move(cbor_build_uint8(1))});
    cbor_map_add(lhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(20)),
                                    .value = cbor_move(cbor_build_uint8(2))});
    cbor_item_t* rhs = cbor_new_definite_map(2);
    cbor_map_add(rhs,
                 (struct cbor_pair){
                     .key = cbor_move(cbor_build_uint8(10)),
                     .value = cbor_move(cbor_build_ctrl(CBOR_CTRL_UNDEF))});
    cbor_map_add(rhs,
                 (struct cbor_pair){.key = cbor_move(cbor_build_uint8(40)),
                                    .value = cbor_move(cbor_build_uint8(4))});

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 5

  // CONCAT TEST 6
  {
    puts("\nCONCAT TEST - STRINGS: lhs=bytestr rhs=textstr");
    const unsigned char lhs_data[] = {0x01, 0x02, 0x03};
    cbor_item_t* lhs = cbor_build_bytestring(lhs_data, sizeof(lhs_data));
    cbor_item_t* rhs = cbor_build_string("456");

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 6

  // CONCAT TEST 7
  {
    puts("\nCONCAT TEST - STRINGS: lhs=textstr rhs=bytestr");
    const unsigned char rhs_data[] = {0x01, 0x02, 0x03};
    cbor_item_t* lhs = cbor_build_string("123");
    cbor_item_t* rhs = cbor_build_bytestring(rhs_data, sizeof(rhs_data));

    cbor_item_t* out = NULL;
    ret = _concatenate(lhs, rhs, &out);
    if (ret != PACKED_ERR_NONE) {
      printf("\nCRASHED: %s\n", describe_error(ret));
    }
    cbor_describe(out, stdout);

    cbor_decref(&lhs);
    cbor_decref(&rhs);
    cbor_decref(&out);
  }
  // CONCAT TEST 7
  */

  return 0;
}
