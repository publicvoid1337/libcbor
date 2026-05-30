/* Packed CBOR unit tests
 * Uses example vectors from examples/cbor_packed.c and public packed APIs
 */

#include "../src/cbor/packed.h"
#include "assertions.h"
#include "cbor.h"
#include "test_allocator.h"

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

static void assert_traverse_result(const unsigned char* data, size_t len,
                                   packed_error_t expected_err,
                                   cbor_type expected_type,
                                   size_t expected_map_size,
                                   const char* expected_string) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(data, len, &res);
  assert_int_equal(res.error.code, CBOR_ERR_NONE);
  assert_non_null(item);

  recursion_info_t rec_inf = _new_rec_info(NULL, item, NULL, 0, false, 0, 0);
  cbor_item_t* new_item = NULL;
  packed_error_t ret = _traverse(rec_inf, &new_item);

  if (new_item != NULL) {
    rec_inf.item = new_item;
  }

  assert_int_equal(ret, expected_err);
  if (expected_err == PACKED_ERR_NONE) {
    assert_int_equal(cbor_typeof(rec_inf.item), expected_type);
    if (expected_type == CBOR_TYPE_MAP) {
      assert_int_equal(cbor_map_size(rec_inf.item), expected_map_size);
    }
    if (expected_string != NULL) {
      assert_int_equal(cbor_string_length(rec_inf.item),
                       strlen(expected_string));
      assert_memory_equal(cbor_string_handle(rec_inf.item), expected_string,
                          strlen(expected_string));
    }
  }

  cbor_decref(&rec_inf.item);
  if (rec_inf.num_active > 0) {
    cbor_decref(&rec_inf.tables[rec_inf.num_active - 1]);
    rec_inf.num_active--;
  }
}

static void test_traverse_example_DATA(void** _state _CBOR_UNUSED) {
  assert_traverse_result(DATA, sizeof(DATA), PACKED_ERR_NONE, CBOR_TYPE_MAP, 1,
                         NULL);
}

static void test_traverse_example_DATA2(void** _state _CBOR_UNUSED) {
  assert_traverse_result(DATA2, sizeof(DATA2), PACKED_ERR_NONE, CBOR_TYPE_UINT,
                         0, NULL);
}

static void test_traverse_example_DATA3(void** _state _CBOR_UNUSED) {
  assert_traverse_result(DATA3, sizeof(DATA3), PACKED_ERR_NONE, CBOR_TYPE_UINT,
                         0, NULL);
}

static void test_traverse_example_DATA4(void** _state _CBOR_UNUSED) {
  /* DATA4 is a nested Tag 6 prefix reference that must be recursively
     unpacked to the text string "$$$". */
  assert_traverse_result(DATA4, sizeof(DATA4), PACKED_ERR_NONE,
                         CBOR_TYPE_STRING, 0, "$$$");
}

static void test_traverse_example_DATA5(void** _state _CBOR_UNUSED) {
  /* DATA5 is a Tag 6 prefix reference to a text string, which also unpacks
     recursively to "$$$". */
  assert_traverse_result(DATA5, sizeof(DATA5), PACKED_ERR_NONE,
                         CBOR_TYPE_STRING, 0, "$$$");
}

static void test_traverse_example_DATA6(void** _state _CBOR_UNUSED) {
  assert_traverse_result(DATA6, sizeof(DATA6),
                         PACKED_ERR_MAX_REF_DEPTH_EXCEEDED, CBOR_TYPE_TAG, 0,
                         NULL);
}

static void test_packing_table_get_multi_table_lookup(
    void** _state _CBOR_UNUSED) {
  cbor_item_t* first_table = cbor_new_definite_array(2);
  assert_true(cbor_array_push(first_table, cbor_move(cbor_build_uint8(1))));
  assert_true(cbor_array_push(first_table, cbor_move(cbor_build_uint8(2))));

  cbor_item_t* second_table = cbor_new_definite_array(2);
  assert_true(cbor_array_push(second_table, cbor_move(cbor_build_uint8(3))));
  assert_true(cbor_array_push(second_table, cbor_move(cbor_build_uint8(4))));

  cbor_item_t* tables[MAX_ACTIVE_TABLES] = {first_table, second_table};
  cbor_item_t* out_item = NULL;
  cbor_item_t* out_table = NULL;
  size_t out_index = 0;
  uint8_t out_num_active = 0;

  packed_error_t err = _packing_table_get(tables, 2, 0, &out_item, &out_table,
                                          &out_index, &out_num_active);
  assert_int_equal(err, PACKED_ERR_NONE);
  assert_int_equal(out_num_active, 2);
  assert_int_equal(out_index, 0);
  assert_true(cbor_isa_uint(out_item));
  assert_uint8(out_item, 3);
  cbor_decref(&out_item);

  err = _packing_table_get(tables, 2, 3, &out_item, &out_table, &out_index,
                           &out_num_active);
  assert_int_equal(err, PACKED_ERR_NONE);
  assert_int_equal(out_num_active, 1);
  assert_int_equal(out_index, 1);
  assert_uint8(out_item, 2);
  cbor_decref(&out_item);

  cbor_decref(&first_table);
  cbor_decref(&second_table);
}

static void test_handle_tag_6_direct(void** _state _CBOR_UNUSED) {
  cbor_item_t* table = cbor_new_definite_array(2);
  assert_true(cbor_array_push(table, cbor_move(cbor_build_uint8(20))));
  assert_true(cbor_array_push(table, cbor_move(cbor_build_uint8(21))));

  cbor_item_t* tag_item = cbor_build_tag(6, cbor_move(cbor_build_uint8(1)));
  assert_non_null(tag_item);

  cbor_item_t* new_item = NULL;
  packed_error_t err = _handle_tag_6(
      &table, 1, (parent_t){.item = NULL, .index = 0, .is_key = false},
      tag_item, &new_item);
  assert_int_equal(err, PACKED_ERR_NONE);
  assert_non_null(new_item);
  assert_true(cbor_isa_uint(new_item));
  assert_uint8(new_item, 21);

  cbor_decref(&new_item);
  cbor_decref(&table);
}

static void test_concatenate_text_and_bytestring(void** _state _CBOR_UNUSED) {
  const unsigned char lhs_data[] = {0x01, 0x02, 0x03};
  cbor_item_t* lhs = cbor_build_bytestring(lhs_data, sizeof(lhs_data));
  cbor_item_t* rhs = cbor_build_string("456");
  assert_non_null(lhs);
  assert_non_null(rhs);

  cbor_item_t* out = NULL;
  packed_error_t err = _concatenate(lhs, rhs, &out, CBOR_TYPE_BYTESTRING);
  assert_int_equal(err, PACKED_ERR_NONE);
  assert_non_null(out);
  assert_true(cbor_isa_bytestring(out));
  assert_true(cbor_bytestring_is_indefinite(out));
  assert_size_equal(cbor_bytestring_length(out), sizeof(lhs_data) + 3);

  cbor_decref(&lhs);
  cbor_decref(&rhs);
  cbor_decref(&out);
}

static void test_join_empty_array_to_string(void** _state _CBOR_UNUSED) {
  cbor_item_t* lhs = cbor_build_string("hello");
  cbor_item_t* rhs = cbor_new_definite_array(0);
  assert_non_null(lhs);
  assert_non_null(rhs);

  cbor_item_t* out = NULL;
  packed_error_t err = _join(lhs, rhs, &out, false);
  assert_int_equal(err, PACKED_ERR_NONE);
  assert_non_null(out);
  assert_true(cbor_isa_string(out));
  assert_true(cbor_string_is_indefinite(out));
  assert_size_equal(cbor_string_length(out), 0);

  cbor_decref(&lhs);
  cbor_decref(&rhs);
  cbor_decref(&out);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_traverse_example_DATA),
      cmocka_unit_test(test_traverse_example_DATA2),
      cmocka_unit_test(test_traverse_example_DATA3),
      cmocka_unit_test(test_traverse_example_DATA4),
      cmocka_unit_test(test_traverse_example_DATA5),
      cmocka_unit_test(test_traverse_example_DATA6),
      // cmocka_unit_test(test_packing_table_get_bounds),
      cmocka_unit_test(test_packing_table_get_multi_table_lookup),
      cmocka_unit_test(test_handle_tag_6_direct),
      // cmocka_unit_test(test_concatenate_arrays),
      // cmocka_unit_test(test_concatenate_maps_overlap),
      // cmocka_unit_test(test_concatenate_text_and_bytestring),
      cmocka_unit_test(test_join_empty_array_to_string),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
