#include "packed.h"
#include <cbor/arrays.h>
#include <cbor/common.h>
#include <cbor/data.h>
#include <cbor/maps.h>
#include <cbor/tags.h>
#include "arrays.h"
#include "floats_ctrls.h"
#include "ints.h"
#include "maps.h"
#include "strings.h"

recursion_info_t _new_rec_info(
    cbor_item_t* new_packing_tables[MAX_ACTIVE_TABLES], cbor_item_t* new_item,
    cbor_item_t* new_parent_item, size_t new_parent_idx, bool new_parent_is_key,
    uint8_t ref_depth, uint8_t num_active) {
  parent_t parent = {.index = new_parent_idx,
                     .item = new_parent_item,
                     .is_key = new_parent_is_key};
  recursion_info_t rec_inf = {.parent = parent,
                              .item = new_item,
                              .ref_depth = ref_depth,
                              .num_active = num_active};
  for (size_t i = 0; i < MAX_ACTIVE_TABLES; i++) {
    rec_inf.tables[i] = new_packing_tables != NULL && i < num_active
                            ? new_packing_tables[i]
                            : NULL;
  }
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
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_BYTESTRING: {
      size_t size = cbor_bytestring_length(target);
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_FLOAT_CTRL: {
      if (cbor_float_ctrl_is_ctrl(target)) {
        int value = cbor_ctrl_value(target);
        printf("%s=%-7s(%-4ld)", identifier, "CTRL", value);
        return;
      } else {
        float value = cbor_float_get_float(target);
        printf("%s=%-7s(%-4ld)", identifier, "FLOAT", value);
      }
      return;
    }
    case CBOR_TYPE_MAP: {
      size_t size = cbor_map_size(target);
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_NEGINT: {
      int value = cbor_get_int(target);
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_UINT: {
      int value = cbor_get_int(target);
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), value);
      return;
    }
    case CBOR_TYPE_STRING: {
      size_t size = cbor_string_length(target);
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), size);
      return;
    }
    case CBOR_TYPE_TAG: {
      size_t value = cbor_tag_value(target);
      printf("%s=%-7s(%-4ld)", identifier, describe_cbor_type(target), value);
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

    cbor_item_t* res;
    unsigned char res_bytes[lhs_size + rhs_size];
    memcpy(res_bytes, lhs_handle, lhs_size);
    memcpy(res_bytes + lhs_size, rhs_handle, rhs_size);

    if (string_out_type == CBOR_TYPE_STRING) {
      *out = cbor_build_stringn(res_bytes, lhs_size + rhs_size);
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
    cbor_type res_type = cbor_typeof(rhs);
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

// bumps refcount of "out_shared_item" by 1
// if the user wishes to chase the returned reference,
// they should use the "out_..." parameters to update the local packing
// evironment
packed_error_t _packing_table_get(cbor_item_t* tables[MAX_ACTIVE_TABLES],
                                  uint8_t num_active, size_t index,
                                  cbor_item_t** out_shared_item,
                                  cbor_item_t** out_table, size_t* out_index,
                                  uint8_t* out_num_active) {
  while (num_active > 0) {
    size_t table_range = cbor_array_size(tables[num_active - 1]);
    if (index < table_range) {
      break;
    }
    index -= table_range;
    num_active--;
  }
  if (num_active == 0) {
    return PACKED_ERR_UNDEFINED_REFERENCE;
  }
  *out_shared_item = cbor_array_get(tables[num_active - 1], index);
  if (*out_shared_item == NULL) {
    return PACKED_ERR_UNDEFINED_REFERENCE;
  }
  if (out_table != NULL) {
    *out_table = tables[num_active - 1];
  }
  if (out_index != NULL) {
    *out_index = index;
  }
  if (out_num_active != NULL) {
    *out_num_active = num_active;
  }
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

packed_error_t _handle_tag_113(parent_t parent, cbor_item_t* item,
                               cbor_item_t** new_root,
                               cbor_item_t** new_table) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("handle_tag_113", item, parent.item);
#endif

  cbor_item_t* arr = cbor_tag_item(item);
  if (arr == NULL || cbor_typeof(arr) != CBOR_TYPE_ARRAY) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t* packing_table = cbor_array_get(arr, 0);
  if (packing_table == NULL || cbor_typeof(packing_table) != CBOR_TYPE_ARRAY) {
    cbor_decref(&arr);
    cbor_decref(&packing_table);
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t* packed_data = cbor_array_get(arr, 1);
  if (packed_data == NULL) {
    cbor_decref(&packing_table);
    cbor_decref(&arr);
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  packed_error_t ret = _replace(parent, item, packed_data);
  CATCH_DECREF_RETURN_1(ret, packed_data, packing_table, arr);

  cbor_decref(&arr);
  /* At this point item is mutated! */
  *new_root = packed_data; /* == item */
  *new_table = packing_table;
  return PACKED_ERR_NONE;
}

packed_error_t _handle_tag_6(cbor_item_t* packing_tables[MAX_ACTIVE_TABLES],
                             uint8_t num_active, parent_t parent,
                             cbor_item_t* item, cbor_item_t** new_item) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("handle_tag_6", item, parent.item);
#endif

  *new_item = NULL;

  cbor_item_t* tag_item = cbor_tag_item(item);
  if (tag_item == NULL) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  if (cbor_typeof(tag_item) == CBOR_TYPE_UINT) {
    size_t index = cbor_get_int(tag_item);
    cbor_item_t* unpacked_item = NULL;
    packed_error_t ret = _packing_table_get(packing_tables, num_active, index,
                                            &unpacked_item, NULL, NULL, NULL);
    if (ret != PACKED_ERR_NONE) {
      cbor_decref(&tag_item);
      return ret;
    }

    if (cbor_typeof(unpacked_item) == CBOR_TYPE_TAG &&
        cbor_tag_value(unpacked_item) == 6) {
      cbor_decref(&tag_item);
      cbor_decref(&unpacked_item);
      return _RESOLVE_THEN_REPLACE;
    }

    packed_error_t resp = _replace(parent, item, unpacked_item);
    if (resp != PACKED_ERR_NONE) {
      cbor_decref(&unpacked_item);
      cbor_decref(&tag_item);
      return resp;
    }
    // item = unpacked_item;

    if (parent.item != NULL) {
      cbor_decref(&unpacked_item);
    }
    cbor_decref(&tag_item);
    *new_item = unpacked_item;
    return resp;
  }
  cbor_decref(&tag_item);

  return _NESTED;
}

packed_error_t _traverse(recursion_info_t rec_inf, cbor_item_t** new_parent) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("traverse", rec_inf.item, rec_inf.parent.item);
#endif

  switch (cbor_typeof(rec_inf.item)) {
    case CBOR_TYPE_ARRAY: {
      for (size_t i = 0; i < cbor_array_size(rec_inf.item); i++) {
        cbor_item_t* child = cbor_array_get(rec_inf.item, i);
        packed_error_t ret = _traverse(
            _new_rec_info(rec_inf.tables, child, rec_inf.item, i, false,
                          rec_inf.ref_depth, rec_inf.num_active),
            &rec_inf.item);
        cbor_decref(&child);
        CATCH_DECREF_RETURN(ret);
      }
      return PACKED_ERR_NONE;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(rec_inf.item);
      /* Pairwise recursive calls with increasing index */
      for (size_t i = 0; i < cbor_map_size(rec_inf.item); i++) {
        packed_error_t ret = _traverse(
            _new_rec_info(rec_inf.tables, pairs[i].key, rec_inf.item, i, true,
                          rec_inf.ref_depth, rec_inf.num_active),
            &rec_inf.item);
        CATCH_DECREF_RETURN(ret);

        ret = _traverse(
            _new_rec_info(rec_inf.tables, pairs[i].value, rec_inf.item, i,
                          false, rec_inf.ref_depth, rec_inf.num_active),
            &rec_inf.item);
        CATCH_DECREF_RETURN(ret);
      }
      return PACKED_ERR_NONE;
    }
    case CBOR_TYPE_FLOAT_CTRL: {
      /* Simple value in range 0-15 => Shared item refrence */
      if (cbor_float_ctrl_is_ctrl(rec_inf.item) &&
          (cbor_ctrl_value(rec_inf.item) >= 0 &&
           cbor_ctrl_value(rec_inf.item) <= 15)) {
        size_t index = cbor_ctrl_value(rec_inf.item);

        cbor_item_t* unpacked_item = NULL;
        packed_error_t ret =
            _packing_table_get(rec_inf.tables, rec_inf.num_active, index,
                               &unpacked_item, NULL, NULL, NULL);
        CATCH_DECREF_RETURN(ret);

        ret = _replace(rec_inf.parent, rec_inf.item, unpacked_item);
        CATCH_DECREF_RETURN(ret, unpacked_item);
        rec_inf.item = unpacked_item;
        if (rec_inf.parent.item == NULL) {
          *new_parent = rec_inf.item;
        }

        /* Recursively unpack */
        rec_inf.ref_depth++;
        if (rec_inf.ref_depth > MAX_REFERENCE_DEPTH) {
          if (rec_inf.parent.item != NULL) {
            cbor_decref(&unpacked_item);
          }
          return PACKED_ERR_MAX_REF_DEPTH_EXCEEDED;
        }

        ret = _traverse(rec_inf, &rec_inf.item);
        if (rec_inf.parent.item == NULL) {
          *new_parent = rec_inf.item;
        } else {
          cbor_decref(&unpacked_item);
        }
        CATCH_DECREF_RETURN(ret);
      }
      return PACKED_ERR_NONE;
    }
    case CBOR_TYPE_TAG: {
      switch (cbor_tag_value(rec_inf.item)) {
        /* Basic Packed CBOR (shared table) */
        case 113: {
          /* Consume the table definition, recieving the packing table and the
           * packed data */
          packed_error_t ret =
              _handle_tag_113(rec_inf.parent, rec_inf.item, &rec_inf.item,
                              &rec_inf.tables[rec_inf.num_active]);
          CATCH_DECREF_RETURN(ret);

          rec_inf.num_active++;
          if (rec_inf.parent.item == NULL) {
            *new_parent = rec_inf.item;
          }
          /* Restart _traverse from the packed data!
           * Note: rec_inf pointers are already updated, since we passed them
           * directly above.*/
          ret = _traverse(rec_inf, &rec_inf.item);
          if (rec_inf.parent.item == NULL) {
            *new_parent = rec_inf.item;
          }
          /* Since _handle_tag_113 gave us ownership of the packing table, we
           * are responisble for feeing it after moving upwards out of its
           * scope */
          cbor_decref(&rec_inf.tables[rec_inf.num_active - 1]);

          rec_inf.num_active--;
          CATCH_DECREF_RETURN(ret);

          if (rec_inf.parent.item == NULL) {
            *new_parent = rec_inf.item;
          }
          return PACKED_ERR_NONE;
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
          /* Get rump and argument */
          size_t idx;
          bool is_straight;
          if (cbor_tag_value(rec_inf.item) < 136) {
            idx = cbor_tag_value(rec_inf.item) - 128;
            is_straight = true;
          } else {
            idx = cbor_tag_value(rec_inf.item) - 136;
            is_straight = false;
          }

          cbor_item_t* argument = NULL;
          packed_error_t ret =
              _packing_table_get(rec_inf.tables, rec_inf.num_active, idx,
                                 &argument, NULL, NULL, NULL);
          CATCH_DECREF_RETURN(ret, argument);
          cbor_item_t* rump = cbor_tag_item(rec_inf.item);
          if (rump == NULL) {
            cbor_decref(&argument);
            return PACKED_ERR_UNEXPECTED_FORMAT;
          }

          /* Determine lhs and rhs as well as function to be applied */
          cbor_item_t* lhs = is_straight ? argument : rump;
          cbor_item_t* rhs = is_straight ? rump : argument;
          cbor_item_t* function_argument = NULL;

          /* TODO: Implement handling of function tags as described in the
           * packed cbor specification */
          int function_id = 0;
          if (cbor_typeof(lhs) == CBOR_TYPE_TAG) {
            function_id = cbor_tag_value(lhs);
            // we do not modify the function tag itself
            function_argument = cbor_tag_item(lhs);
            if (function_argument == NULL) {
              cbor_decref(&argument);
              cbor_decref(&rump);
              return PACKED_ERR_UNEXPECTED_FORMAT;
            }
            lhs = function_argument;
          }

          /* Unpack both recursively */
          ret = _traverse(
              _new_rec_info(rec_inf.tables, rump, rec_inf.item, 0, false,
                            rec_inf.ref_depth, rec_inf.num_active),
              &rump);
          CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument);

          ret = _traverse(
              _new_rec_info(rec_inf.tables, argument, rec_inf.item, 0, false,
                            rec_inf.ref_depth, rec_inf.num_active),
              &argument);
          CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument);

          /* Apply function */
          cbor_item_t* res = NULL;
          switch (function_id) {
            /* No function tag specified - concatenate */
            case 0: {
              ret = _concatenate(lhs, rhs, &res, cbor_typeof(rump));
              CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument,
                                    res);
              break;
            }
            /* Interchanged join */
            case 105: {
              ret = _join(rhs, lhs, &res);
              CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument,
                                    res);
              break;
            }
            /* Join*/
            case 106: {
              ret = _join(lhs, rhs, &res);
              CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument,
                                    res);
              break;
            }
            /* Record */
            case 114: {
              ret = _record(lhs, rhs, &res);
              CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument,
                                    res);
              break;
            }
          }

          /* Replace reference with result of function application */
          ret = _replace(rec_inf.parent, rec_inf.item, res);
          CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument, res);
          rec_inf.item = res;
          if (rec_inf.parent.item == NULL) {
            *new_parent = rec_inf.item;
          }

          cbor_decref(&rec_inf.item);
          cbor_decref(&argument);
          cbor_decref(&rump);
          if (function_argument != NULL) {
            cbor_decref(&function_argument);
          }
          return PACKED_ERR_NONE;
        }
        case 6: {
          cbor_item_t* tag_new_item = NULL;
          packed_error_t ret =
              _handle_tag_6(rec_inf.tables, rec_inf.num_active, rec_inf.parent,
                            rec_inf.item, &tag_new_item);
          if (ret == _NESTED) {
            cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
            ret = _traverse(
                _new_rec_info(rec_inf.tables, tag_child, rec_inf.item, 0, false,
                              rec_inf.ref_depth, rec_inf.num_active),
                &rec_inf.item);
            cbor_decref(&tag_child);
            CATCH_DECREF_RETURN(ret);

            ret = _handle_tag_6(rec_inf.tables, rec_inf.num_active,
                                rec_inf.parent, rec_inf.item, &tag_new_item);
            if (ret == _NESTED) {
              ret = _replace(rec_inf.parent, rec_inf.item,
                             cbor_move(cbor_tag_item(rec_inf.item)));
              CATCH_DECREF_RETURN(ret);

              return PACKED_ERR_NONE;
            } else if (ret == _RESOLVE_THEN_REPLACE) {
              return _traverse(rec_inf, new_parent);  // restart process
            } else if (ret != PACKED_ERR_NONE) {
              return ret;
            }
          }
          if (ret == _RESOLVE_THEN_REPLACE) {
            rec_inf.ref_depth++;
            if (rec_inf.ref_depth > MAX_REFERENCE_DEPTH) {
              return PACKED_ERR_MAX_REF_DEPTH_EXCEEDED;
            }
            size_t index = cbor_get_int(cbor_move(cbor_tag_item(rec_inf.item)));

            cbor_item_t* unpacked_item = NULL;
            cbor_item_t* unpacked_item_table = NULL;
            size_t unpacked_item_index = 0;
            uint8_t unpacked_item_num_active = 0;
            ret = _packing_table_get(rec_inf.tables, rec_inf.num_active, index,
                                     &unpacked_item, &unpacked_item_table,
                                     &unpacked_item_index,
                                     &unpacked_item_num_active);
            CATCH_DECREF_RETURN(ret);

            ret = _traverse(
                _new_rec_info(rec_inf.tables, unpacked_item,
                              unpacked_item_table, unpacked_item_index, false,
                              rec_inf.ref_depth, unpacked_item_num_active),
                &rec_inf.item);
            cbor_decref(&unpacked_item);
            CATCH_DECREF_RETURN(ret);

            ret = _handle_tag_6(rec_inf.tables, rec_inf.num_active,
                                rec_inf.parent, rec_inf.item, &tag_new_item);
            CATCH_DECREF_RETURN(ret);
          }
          if (ret == PACKED_ERR_NONE) {
            if (rec_inf.parent.item == NULL) {
              *new_parent = tag_new_item;
              return PACKED_ERR_NONE;
            }
          }
          return ret;
        }
        default: {
          /* Tag not specified by packed cbor */
          cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
          packed_error_t ret = _traverse(
              _new_rec_info(rec_inf.tables, tag_child, rec_inf.item, 0, false,
                            rec_inf.ref_depth, rec_inf.num_active),
              &rec_inf.item);
          cbor_decref(&tag_child);
          CATCH_DECREF_RETURN(ret);

          return PACKED_ERR_NONE;
        }
      }
    }
    default: {
      /* Normal leaf node */
      return PACKED_ERR_NONE;
    }
  }
}
