#include "packed.h"
#include <assert.h>
#include <cbor/arrays.h>
#include <cbor/common.h>
#include <cbor/data.h>
#include <cbor/maps.h>
#include <cbor/tags.h>
#include <stdbool.h>
#include <stdio.h>
#include "arrays.h"
#include "floats_ctrls.h"
#include "ints.h"
#include "maps.h"
#include "strings.h"
#include "tags.h"

recursion_info_t _new_rec_info(packing_ctx_t new_packing_ctx,
                               cbor_item_t* new_item,
                               cbor_item_t* new_parent_item,
                               size_t new_parent_idx, bool new_parent_is_key,
                               uint8_t ref_depth) {
  parent_t parent = {.index = new_parent_idx,
                     .item = new_parent_item,
                     .is_key = new_parent_is_key};
  recursion_info_t rec_inf = {.parent = parent,
                              .item = new_item,
                              .ref_depth = ref_depth,
                              .packing_ctx = new_packing_ctx};
  return rec_inf;
}

const char* describe_error(packed_error_t error) {
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
    case PACKED_ERR_MAX_REF_DEPTH_EXCEEDED:
      return "Maximum reference depth was exceeded";
    case _NESTED:
      return "Nested structure requires additional handling";
    default:
      return "Unknown error";
  }
}

const char* describe_cbor_type(cbor_item_t* item) {
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
    printf("%s=NULL         ", identifier);
    return;
  }
  switch (cbor_typeof(target)) {
    case CBOR_TYPE_ARRAY: {
      size_t size = cbor_array_size(target);
      printf("%s=%-7s(%-4zu)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_BYTESTRING: {
      size_t size = cbor_bytestring_length(target);
      printf("%s=%-7s(%-4zu)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_FLOAT_CTRL: {
      if (cbor_float_ctrl_is_ctrl(target)) {
        int value = cbor_ctrl_value(target);
        printf("%s=%-7s(%-4d)", identifier, "CTRL", value);
        return;
      } else {
        float value = cbor_float_get_float(target);
        printf("%s=%-7s(%-4f)", identifier, "FLOAT", value);
      }
      return;
    }
    case CBOR_TYPE_MAP: {
      size_t size = cbor_map_size(target);
      printf("%s=%-7s(%-4zu)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_NEGINT: {
      int value = cbor_get_int(target);
      printf("%s=%-7s(%-4d)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_UINT: {
      int value = cbor_get_int(target);
      printf("%s=%-7s(%-4d)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_STRING: {
      size_t size = cbor_string_length(target);
      printf("%s=%-7s(%-4zu)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_TAG: {
      size_t value = cbor_tag_value(target);
      printf("%s=%-7s(%-4zu)", identifier, describe_cbor_type(target), value);
      return;
    }
  }
}

packed_error_t _concatenate(cbor_item_t* lhs, cbor_item_t* rhs,
                            cbor_item_t** out, cbor_type string_out_type) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("concatenate", NULL, NULL);
#endif

  if (cbor_typeof(lhs) == CBOR_TYPE_ARRAY &&
      cbor_typeof(rhs) == CBOR_TYPE_ARRAY) {
    size_t lhs_size = cbor_array_size(lhs);
    size_t rhs_size = cbor_array_size(rhs);
    cbor_item_t* new_arr = cbor_new_definite_array(lhs_size + rhs_size);
    if (new_arr == NULL) {
      // TODO: better semantics
      return PACKED_ERR_NOT_SUPPORTED;
    }

    size_t i = 0;
    while (i < lhs_size + rhs_size) {
      cbor_item_t* target = i < lhs_size ? lhs : rhs;
      size_t idx = i < lhs_size ? i : i - lhs_size;
      cbor_item_t* item = cbor_array_get(target, idx);
      // maybe mem leak
      if (!cbor_array_push(new_arr, cbor_move(item))) {
        cbor_decref(&item);
        cbor_decref(&new_arr);
        // TODO: better semantics
        return PACKED_ERR_NOT_SUPPORTED;
      }
      i++;
    }
    *out = new_arr;
    return PACKED_ERR_NONE;
  } else if (cbor_typeof(lhs) == CBOR_TYPE_MAP &&
             cbor_typeof(rhs) == CBOR_TYPE_MAP) {
    struct cbor_pair* rhs_handle = cbor_map_handle(rhs);
    size_t rhs_size = cbor_map_size(rhs);
    struct cbor_pair* lhs_handle = cbor_map_handle(lhs);
    size_t lhs_size = cbor_map_size(lhs);
    cbor_item_t* res = cbor_new_definite_map(lhs_size + rhs_size);

    for (size_t i = 0; i < rhs_size; i++) {
      if (cbor_is_undef(rhs_handle[i].value)) {
        continue;
      }
      assert(cbor_map_add(res, rhs_handle[i]));
    }
    for (size_t j = 0; j < lhs_size; j++) {
      bool skip = false;
      for (size_t i = 0; i < rhs_size; i++) {
        if (cbor_structurally_equal(lhs_handle[j].key, rhs_handle[i].key)) {
          skip = true;
          break;
        }
      }
      if (skip) continue;
      assert(cbor_map_add(res, lhs_handle[j]));
    }

    *out = res;
    return PACKED_ERR_NONE;
  } else if ((cbor_typeof(lhs) == CBOR_TYPE_STRING ||
              cbor_typeof(lhs) == CBOR_TYPE_BYTESTRING) &&
             (cbor_typeof(rhs) == CBOR_TYPE_STRING ||
              cbor_typeof(rhs) == CBOR_TYPE_BYTESTRING)) {
    cbor_mutable_data lhs_handle = cbor_typeof(lhs) == CBOR_TYPE_STRING
                                       ? cbor_string_handle(lhs)
                                       : cbor_bytestring_handle(lhs);
    size_t lhs_size = cbor_typeof(lhs) == CBOR_TYPE_STRING
                          ? cbor_string_length(lhs)
                          : cbor_bytestring_length(lhs);
    cbor_mutable_data rhs_handle = cbor_typeof(rhs) == CBOR_TYPE_STRING
                                       ? cbor_string_handle(rhs)
                                       : cbor_bytestring_handle(rhs);
    size_t rhs_size = cbor_typeof(rhs) == CBOR_TYPE_STRING
                          ? cbor_string_length(rhs)
                          : cbor_bytestring_length(rhs);

    unsigned char res_bytes[lhs_size + rhs_size];
    memcpy(res_bytes, lhs_handle, lhs_size);
    memcpy(res_bytes + lhs_size, rhs_handle, rhs_size);

    if (string_out_type == CBOR_TYPE_STRING) {
      *out = cbor_build_stringn((const char*)res_bytes, lhs_size + rhs_size);
    } else if (string_out_type == CBOR_TYPE_BYTESTRING) {
      *out = cbor_build_bytestring(res_bytes, lhs_size + rhs_size);
    } else {
      return PACKED_ERR_UNEXPECTED_FORMAT;
    }
    return PACKED_ERR_NONE;
  } else if (((cbor_typeof(lhs) == CBOR_TYPE_STRING ||
               cbor_typeof(lhs) == CBOR_TYPE_STRING) &&
              cbor_typeof(rhs) == CBOR_TYPE_ARRAY) ||
             ((cbor_typeof(rhs) == CBOR_TYPE_STRING ||
               cbor_typeof(rhs) == CBOR_TYPE_STRING) &&
              cbor_typeof(lhs) == CBOR_TYPE_ARRAY)) {
    /* TODO: this parameter is never used which may lead to wrong results */
    cbor_item_t *_lhs, *_rhs;
    _lhs = (cbor_typeof(lhs) == CBOR_TYPE_STRING) ? lhs : rhs;
    _rhs = (cbor_typeof(rhs) == CBOR_TYPE_STRING) ? lhs : rhs;

    return _join(_lhs, _rhs, out);
  } else {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }
}

packed_error_t _join(cbor_item_t* lhs, cbor_item_t* rhs, cbor_item_t** out) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("join", NULL, NULL);
#endif
  // lhs = concatable item
  // rhs = array of concatable ite
  if (cbor_typeof(rhs) != CBOR_TYPE_ARRAY) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t** rhs_handle = cbor_array_handle(rhs);
  size_t rhs_length = cbor_array_size(rhs);
  if (rhs_length == 0) {
    switch (cbor_typeof(lhs)) {
      case CBOR_TYPE_STRING:
        *out = cbor_new_indefinite_string();
        return PACKED_ERR_NONE;
      case CBOR_TYPE_BYTESTRING:
        *out = cbor_new_indefinite_bytestring();
        return PACKED_ERR_NONE;
      case CBOR_TYPE_ARRAY:
        *out = cbor_new_indefinite_array();
        return PACKED_ERR_NONE;
      case CBOR_TYPE_MAP:
        *out = cbor_new_indefinite_map();
        return PACKED_ERR_NONE;
      default:
        return PACKED_ERR_UNEXPECTED_FORMAT;
    }
  }
  if (rhs_length == 1) {
    *out = cbor_incref(rhs_handle[0]);
    return PACKED_ERR_NONE;
  }

  cbor_item_t* res = cbor_incref(rhs_handle[0]);
  cbor_type out_type = cbor_typeof(res);
  for (size_t i = 1; i < rhs_length; i++) {
    packed_error_t err;
    if (i % 2 == 1) {
      cbor_item_t* next = NULL;
      err = _concatenate(res, lhs, &next, out_type);
      cbor_decref(&res);
      CATCH_DECREF_RETURN(err);
      res = next;
    }
    cbor_item_t* next = NULL;
    err = _concatenate(res, rhs_handle[i], &next, out_type);
    cbor_decref(&res);
    CATCH_DECREF_RETURN(err);
    res = next;
  }

  *out = res;
  return PACKED_ERR_NONE;
}

packed_error_t _record(cbor_item_t* lhs, cbor_item_t* rhs, cbor_item_t** out) {
  if (cbor_typeof(lhs) != CBOR_TYPE_ARRAY ||
      cbor_typeof(rhs) != CBOR_TYPE_ARRAY) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t** lhs_handle = cbor_array_handle(lhs);
  size_t lhs_size = cbor_array_size(lhs);
  cbor_item_t** rhs_handle = cbor_array_handle(rhs);
  size_t rhs_size = cbor_array_size(rhs);
  if (rhs_size > lhs_size) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t* res = cbor_new_definite_map(lhs_size);
  for (size_t i = 0; i < lhs_size; i++) {
    if (i >= rhs_size) {
      continue;
    }
    if (cbor_is_undef(rhs_handle[i])) {
      continue;
    }

    struct cbor_pair pair = {.key = lhs_handle[i], .value = rhs_handle[i]};
    assert(cbor_map_add(res, pair));
  }

  *out = res;
  return PACKED_ERR_NONE;
}

packed_error_t _splice(cbor_item_t* arg,
                       parent_t parent __attribute__((unused)),
                       cbor_item_t** out) {
  cbor_item_t* res = cbor_new_indefinite_array();
  for (size_t i = 0; i < cbor_array_size(arg); i++) {
    cbor_item_t* child = cbor_array_get(arg, i);
    if (cbor_typeof(child) == CBOR_TYPE_TAG && cbor_tag_value(child) == 1115) {
      cbor_item_t* tag_arg = cbor_tag_item(child);
      if (tag_arg == NULL || cbor_typeof(tag_arg) != CBOR_TYPE_ARRAY) {
        if (tag_arg != NULL) {
          cbor_decref(&tag_arg);
        }
        cbor_decref(&child);
        cbor_decref(&res);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      for (size_t j = 0; j < cbor_array_size(tag_arg); j++) {
        cbor_item_t* spliced_child = cbor_array_get(tag_arg, j);
        assert(cbor_array_push(res, spliced_child));
        cbor_decref(&spliced_child);
      }
      cbor_decref(&tag_arg);
    } else {
      assert(cbor_array_push(res, child));
    }
    cbor_decref(&child);
  }

  *out = res;
  return PACKED_ERR_NONE;
}

// bumps refcount of "out_shared_item" by 1
// if the user wishes to chase the returned reference,
// they should use the "out_..." parameters to update the local packing
// evironment
packed_error_t _packing_table_get(packing_ctx_t packing_ctx, size_t index,
                                  bool is_arg_ref,
                                  cbor_item_t** out_shared_item,
                                  cbor_item_t** out_table, size_t* out_index,
                                  uint8_t* out_num_active) {
  int curr_table_idx = packing_ctx.num_active - 1;
  while (curr_table_idx > 0) {
    size_t table_range;
    if (packing_ctx.tables[curr_table_idx].is_basic) {
      table_range = cbor_array_size(
          packing_ctx.tables[curr_table_idx].basic.combined_table);
    } else {
      table_range =
          is_arg_ref
              ? cbor_array_size(
                    packing_ctx.tables[curr_table_idx].split.argument_table)
              : cbor_array_size(
                    packing_ctx.tables[curr_table_idx].split.shareditem_table);
    }
    if (index < table_range) {
      break;
    }
    index -= table_range;
    curr_table_idx--;
  }
  if (curr_table_idx < 0) {
    return PACKED_ERR_UNDEFINED_REFERENCE;
  }

  if (packing_ctx.tables[curr_table_idx].is_basic) {
    *out_shared_item = cbor_array_get(
        packing_ctx.tables[curr_table_idx].basic.combined_table, index);
  } else {
    *out_shared_item =
        is_arg_ref
            ? cbor_array_get(
                  packing_ctx.tables[curr_table_idx].split.argument_table,
                  index)
            : cbor_array_get(
                  packing_ctx.tables[curr_table_idx].split.shareditem_table,
                  index);
  }
  if (*out_shared_item == NULL) {
    return PACKED_ERR_UNDEFINED_REFERENCE;
  }

  if (out_table != NULL) {
    if (packing_ctx.tables[curr_table_idx].is_basic) {
      *out_table = packing_ctx.tables[curr_table_idx].basic.combined_table;
    } else {
      *out_table =
          is_arg_ref
              ? packing_ctx.tables[curr_table_idx].split.argument_table
              : packing_ctx.tables[curr_table_idx].split.shareditem_table;
    }
  }
  /*
  if (out_index != NULL) {
    *out_index = index;
  }
  if (out_num_active != NULL) {
    *out_num_active = num_active;
  }
  */

  return PACKED_ERR_NONE;
}

packed_error_t _replace(parent_t parent, cbor_item_t* old_item,
                        cbor_item_t* new_item) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("replace", old_item, parent.item);
#endif

  if (new_item == NULL) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }
  if (parent.item == NULL) {
    cbor_decref(&old_item);
    return PACKED_ERR_NONE;
  }
  switch (cbor_typeof(parent.item)) {
    case CBOR_TYPE_ARRAY: {
      cbor_item_t* old_item = cbor_array_get(parent.item, parent.index);
      if (!cbor_array_replace(parent.item, parent.index, new_item)) {
        if (old_item != NULL) {
          cbor_decref(&old_item);
        }
        return PACKED_ERR_OUT_OF_BOUNDS;
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
      return PACKED_ERR_UNEXPECTED_FORMAT;
  }
  return PACKED_ERR_NONE;
}

// If no err: if basic cbor *new_table_2 will still be Null (unchanged)
packed_error_t _consume_table_definition(parent_t parent, cbor_item_t* item,
                                         cbor_item_t** new_root,
                                         cbor_item_t** new_table_1,
                                         cbor_item_t** new_table_2) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("consume_table", item, parent.item);
#endif

  cbor_item_t* arr = cbor_tag_item(item);
  if (arr == NULL || cbor_typeof(arr) != CBOR_TYPE_ARRAY) {
    cbor_decref(&arr);
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t *packing_table_1, *packing_table_2, *packed_data = NULL;
  switch (cbor_array_size(arr)) {
    /* Basic cbor */
    case 2: {
      packing_table_1 = cbor_array_get(arr, 0);
      if (packing_table_1 == NULL ||
          cbor_typeof(packing_table_1) != CBOR_TYPE_ARRAY) {
        cbor_decref(&packing_table_1);
        cbor_decref(&arr);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }

      packed_data = cbor_array_get(arr, 1);
      if (packed_data == NULL) {
        cbor_decref(&packed_data);
        cbor_decref(&packing_table_1);
        cbor_decref(&arr);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      break;
    }
    /* Split basic cbor*/
    case 3: {
      packing_table_1 = cbor_array_get(arr, 0);
      if (packing_table_1 == NULL ||
          cbor_typeof(packing_table_1) != CBOR_TYPE_ARRAY) {
        cbor_decref(&packing_table_1);
        cbor_decref(&arr);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }

      packing_table_2 = cbor_array_get(arr, 1);
      if (packing_table_2 == NULL ||
          cbor_typeof(packing_table_2) != CBOR_TYPE_ARRAY) {
        cbor_decref(&packing_table_2);
        cbor_decref(&packing_table_1);
        cbor_decref(&arr);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }

      packed_data = cbor_array_get(arr, 2);
      if (packed_data == NULL) {
        cbor_decref(&packed_data);
        cbor_decref(&packing_table_2);
        cbor_decref(&packing_table_1);
        cbor_decref(&arr);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      break;
    }
    default: {
      cbor_decref(&arr);
      return PACKED_ERR_UNEXPECTED_FORMAT;
    }
  }

  packed_error_t ret = _replace(parent, item, packed_data);
  cbor_decref(&arr);
  CATCH_DECREF_RETURN_1(ret, packed_data, packing_table_1, packing_table_2);

  /* At this point item is mutated! */
  *new_root = packed_data;
  *new_table_1 = packing_table_1;
  if (new_table_2 != NULL) {
    *new_table_2 = packing_table_2;
  }

  return PACKED_ERR_NONE;
}

packed_error_t _resolve_shared_item_ref(packing_ctx_t packing_ctx, size_t index,
                                        size_t ref_depth, parent_t parent,
                                        cbor_item_t* item,
                                        cbor_item_t** new_item) {
  if (ref_depth > MAX_REFERENCE_DEPTH) {
    return PACKED_ERR_MAX_REF_DEPTH_EXCEEDED;
  }

  // TODO: deep-copy packing table argument
  cbor_item_t* unpacked_item = NULL;
  packed_error_t ret = _packing_table_get(packing_ctx, index, false,
                                          &unpacked_item, NULL, NULL, NULL);
  CATCH_DECREF_RETURN(ret);

  ret = _replace(parent, item, unpacked_item);
  CATCH_DECREF_RETURN(ret, unpacked_item);
  item = unpacked_item;
  if (parent.item != NULL) {
    cbor_decref(&unpacked_item);
  }

  /* Check integration tags */
  if (cbor_typeof(item) == CBOR_TYPE_TAG) {
    switch (cbor_tag_value(item)) {
      case 1115: {
        /* Our parent must be an array */
        cbor_decref(&unpacked_item);
        return _TAG_1115;
      }
    }
  }

  *new_item = item;
  return PACKED_ERR_NONE;
}

#define _NEO_TRAVERSE_2(rec_inf, replace_ctx, old_item)                \
  do {                                                                 \
    cbor_item_t* recv_replacee = NULL;                                 \
    neo_tabledef_t recv_tabledef;                                      \
    packed_error_t ret =                                               \
        _neo_traverse(rec_inf, &recv_replacee, &recv_tabledef);        \
                                                                       \
    neo_tabledef_t* starting_table = rec_inf.tabledef;                 \
    while (ret == _CURR_REPLACED) {                                    \
      assert(_replace(replace_ctx, old_item, recv_replacee) ==         \
             PACKED_ERR_NONE);                                         \
      rec_inf.curr = recv_replacee;                                    \
                                                                       \
      if (recv_tabledef.is_basic &&                                    \
          recv_tabledef.data.combined_table != NULL) {                 \
        neo_tabledef_t* new_tabledef = malloc(sizeof(neo_tabledef_t)); \
        *new_tabledef = recv_tabledef;                                 \
        new_tabledef->prev = rec_inf.tabledef;                         \
        rec_inf.tabledef = new_tabledef;                               \
      }                                                                \
                                                                       \
      ret = _neo_traverse(rec_inf, &recv_replacee, &recv_tabledef);    \
    }                                                                  \
                                                                       \
    while (rec_inf.tabledef != starting_table) {                       \
      cbor_decref(&rec_inf.tabledef->data.combined_table);             \
      neo_tabledef_t* next = rec_inf.tabledef->prev;                   \
      free(rec_inf.tabledef);                                          \
      rec_inf.tabledef = next;                                         \
    }                                                                  \
                                                                       \
    if (ret != PACKED_ERR_NONE) {                                      \
      return ret;                                                      \
    }                                                                  \
  } while (0)

#if PACKED_ENABLE_DEBUG
size_t ctr = 0;
#endif

// packed_error_t _neo_print_dbg(const char* module_name, cbor_item_t* curr, )

packed_error_t _neo_traverse(neo_rec_inf_t rec_inf, cbor_item_t** replacee,
                             neo_tabledef_t* new_table) {
  switch (cbor_typeof(rec_inf.curr)) {
    case CBOR_TYPE_ARRAY: {
      for (size_t i = 0; i < cbor_array_size(rec_inf.curr); i++) {
        cbor_item_t* child = cbor_array_get(rec_inf.curr, i);
        neo_rec_inf_t next_rec_inf = {.curr = child,
                                      .depth = rec_inf.depth,
                                      .tabledef = rec_inf.tabledef};
        parent_t replace_ctx = {.item = rec_inf.curr, .index = i};
        _NEO_TRAVERSE_2(next_rec_inf, replace_ctx, child);
        cbor_decref(&child);
      }
      return PACKED_ERR_NONE;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(rec_inf.curr);
      for (size_t i = 0; i < cbor_map_size(rec_inf.curr); i++) {
        neo_rec_inf_t next_rec_inf = {.curr = pairs[i].key,
                                      .depth = rec_inf.depth,
                                      .tabledef = rec_inf.tabledef};
        parent_t replace_ctx = {
            .item = rec_inf.curr, .index = i, .is_key = true};
        _NEO_TRAVERSE_2(rec_inf, replace_ctx, pairs[i].key);

        replace_ctx.is_key = false;
        _NEO_TRAVERSE_2(rec_inf, replace_ctx, pairs[i].value);
      }
      return PACKED_ERR_NONE;
    }
    case CBOR_TYPE_TAG: {
      switch (cbor_tag_value(rec_inf.curr)) {
        case 113: {
          cbor_item_t* enclosing_arr = cbor_tag_item(rec_inf.curr);
          assert(enclosing_arr != NULL);
          assert(cbor_array_size(enclosing_arr) == 2);

          cbor_item_t* table = cbor_array_get(enclosing_arr, 0);
          assert(table != NULL);
          assert(cbor_typeof(table) == CBOR_TYPE_ARRAY);
          if (rec_inf.tabledef == NULL) {
            neo_tabledef_t tabledef = {
                .data.combined_table = table, .is_basic = true, .prev = NULL};
            *new_table = tabledef;
          } else {
            neo_tabledef_t tabledef = {.data.combined_table = table,
                                       .is_basic = true,
                                       .prev = rec_inf.tabledef};
            *new_table = tabledef;
          }

          cbor_item_t* packed_data = cbor_array_get(enclosing_arr, 1);
          assert(packed_data != NULL);
          *replacee = packed_data;

          cbor_decref(&enclosing_arr);
          return _CURR_REPLACED;
        }
      }
    }
    default: {
      /* Normal leaf node */
      return PACKED_ERR_NONE;
    }
  }
}

cbor_item_t* cbor_unpack(cbor_item_t* target,
                         neo_tabledef_t* global_packing_table) {
  // TODO: Should probably deep copy target
  neo_rec_inf_t rec_inf = {
      .curr = target, .depth = 0, .tabledef = global_packing_table};
  cbor_item_t* recv_replacee = NULL;
  neo_tabledef_t recv_tabledef;
  packed_error_t ret = _neo_traverse(rec_inf, &recv_replacee, &recv_tabledef);

  while (ret == _CURR_REPLACED) {
    cbor_decref(&rec_inf.curr);
    rec_inf.curr = recv_replacee;
    if (recv_tabledef.is_basic && recv_tabledef.data.combined_table != NULL) {
      neo_tabledef_t* new_tabledef = malloc(sizeof(neo_tabledef_t));
      *new_tabledef = recv_tabledef;
      new_tabledef->prev = rec_inf.tabledef;
      rec_inf.tabledef = new_tabledef;
    }
    ret = _neo_traverse(rec_inf, &recv_replacee, &recv_tabledef);
  }
  while (rec_inf.tabledef != global_packing_table) {
    cbor_decref(&rec_inf.tabledef->data.combined_table);
    neo_tabledef_t* next = rec_inf.tabledef->prev;
    free(rec_inf.tabledef);
    rec_inf.tabledef = next;
  }

  if (ret != PACKED_ERR_NONE) {
    return NULL;
  } else {
    return rec_inf.curr;
  }
}