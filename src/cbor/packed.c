#include "packed.h"

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

packed_error_t _concatenate(cbor_item_t* lhs, cbor_item_t* rhs,
                            cbor_item_t** out) {
  if (cbor_typeof(lhs) == CBOR_TYPE_ARRAY &&
      cbor_typeof(rhs) == CBOR_TYPE_ARRAY) {
    size_t lhs_size = cbor_array_size(lhs);
    size_t rhs_size = cbor_array_size(rhs);
    cbor_item_t* new_arr = cbor_new_definite_array(lhs_size + rhs_size);
    if (new_arr == NULL) {
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
        return PACKED_ERR_NOT_SUPPORTED;
      }
      i++;
    }
    *out = new_arr;
    return PACKED_ERR_NONE;
  } else if (cbor_typeof(lhs) == CBOR_TYPE_MAP &&
             cbor_typeof(rhs) == CBOR_TYPE_MAP) {
    // This returns a indefinite map, which is maybe wrong
    cbor_item_t* res = cbor_new_indefinite_map();

    struct cbor_pair* rhs_handle = cbor_map_handle(rhs);
    size_t rhs_size = cbor_map_size(rhs);
    for (size_t i = 0; i < rhs_size; i++) {
      if (cbor_is_undef(rhs_handle[i].value)) {
        continue;
      }
      cbor_map_add(res, rhs_handle[i]);
    }

    struct cbor_pair* lhs_handle = cbor_map_handle(lhs);
    size_t lhs_size = cbor_map_size(lhs);
    for (size_t j = 0; j < lhs_size; j++) {
      //
      bool skip = false;
      for (size_t i = 0; i < rhs_size; i++) {
        if (cbor_structurally_equal(lhs_handle[j].key, rhs_handle[i].key)) {
          skip = true;
          break;
        }
      }
      if (skip) continue;
      cbor_map_add(res, lhs_handle[j]);
    }

    *out = res;
    return PACKED_ERR_NONE;
  } else if ((cbor_typeof(lhs) == CBOR_TYPE_STRING ||
              cbor_typeof(lhs) == CBOR_TYPE_BYTESTRING) &&
             (cbor_typeof(rhs) == CBOR_TYPE_STRING ||
              cbor_typeof(rhs) == CBOR_TYPE_BYTESTRING)) {
    // lhs = rump, rhs = argument
    cbor_data rhs_handle;
    size_t rhs_length;
    if (cbor_typeof(rhs) == CBOR_TYPE_BYTESTRING) {
      /* Only supporst definite length strings right now */
      rhs_handle = cbor_bytestring_handle(rhs);
      rhs_length = cbor_bytestring_length(rhs);
    } else {
      /* Only supporst definite length strings right now */
      rhs_handle = cbor_string_handle(rhs);
      rhs_length = cbor_string_length(rhs);
    }

    cbor_item_t* res;
    if (cbor_typeof(lhs) == CBOR_TYPE_BYTESTRING) {
      res = cbor_new_indefinite_bytestring();
      cbor_item_t* lhs_handle = cbor_build_bytestring(
          cbor_bytestring_handle(lhs), cbor_bytestring_length(lhs));
      if (!cbor_bytestring_add_chunk(res, lhs_handle)) {
        cbor_decref(&lhs_handle);
        cbor_decref(&res);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      cbor_decref(&lhs_handle);

      if (!cbor_bytestring_add_chunk(
              res, cbor_move(cbor_build_bytestring(rhs_handle, rhs_length)))) {
        cbor_decref(&lhs_handle);
        cbor_decref(&res);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      *out = res;
      return PACKED_ERR_NONE;
    } else {
      res = cbor_new_indefinite_string();
      cbor_item_t* lhs_str = cbor_build_stringn(
          (const char*)cbor_string_handle(lhs), cbor_string_length(lhs));
      if (!cbor_string_add_chunk(res, lhs_str)) {
        cbor_decref(&lhs_str);
        cbor_decref(&res);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      cbor_decref(&lhs_str);

      cbor_item_t* rhs_str =
          cbor_build_stringn((const char*)rhs_handle, rhs_length);
      // NOT VALID UTF-8 CHECK, maybe wrong
      if (cbor_string_codepoint_count(rhs_str) == 0) {
        cbor_decref(&rhs_str);
        cbor_decref(&res);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }

      if (!cbor_string_add_chunk(res, cbor_move(rhs_str))) {
        cbor_decref(&res);
        return PACKED_ERR_UNEXPECTED_FORMAT;
      }
      *out = res;
      return PACKED_ERR_NONE;
    }
  } else if (((cbor_typeof(lhs) == CBOR_TYPE_STRING ||
               cbor_typeof(lhs) == CBOR_TYPE_STRING) &&
              cbor_typeof(rhs) == CBOR_TYPE_ARRAY) ||
             ((cbor_typeof(rhs) == CBOR_TYPE_STRING ||
               cbor_typeof(rhs) == CBOR_TYPE_STRING) &&
              cbor_typeof(lhs) == CBOR_TYPE_ARRAY)) {
    // equivalent to join function
    return PACKED_ERR_NOT_SUPPORTED;
  } else {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }
}

packed_error_t _join(cbor_item_t* lhs, cbor_item_t* rhs, cbor_item_t** out,
                     bool inverted) {
  // lhs = concatable item
  // rhs = array of concatable item
  if (inverted) {
    cbor_item_t* _tmp = lhs;
    lhs = rhs;
    rhs = _tmp;
  }

  if (cbor_typeof(rhs) != CBOR_TYPE_ARRAY) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  size_t rhs_length = cbor_array_size(rhs);
  cbor_item_t** rhs_handle = cbor_array_handle(rhs);
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

  cbor_item_t* res = rhs_handle[0];
  for (size_t i = 1; i < rhs_length; i++) {
    packed_error_t err;
    if (i % 2 == 1) {
      err = _concatenate(res, lhs, &res);
      if (err != PACKED_ERR_NONE) {
        return err;
      }
    }
    err = _concatenate(res, rhs_handle[i], &res);
    if (err != PACKED_ERR_NONE) {
      return err;
    }
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
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  cbor_item_t* packed_data = cbor_array_get(arr, 1);
  if (packed_data == NULL) {
    cbor_decref(&packing_table);
    cbor_decref(&arr);
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }

  packed_error_t ret = _replace(parent, item, packed_data);
  if (ret != PACKED_ERR_NONE) {
    cbor_decref(&packed_data);
    cbor_decref(&packing_table);
    cbor_decref(&arr);
    return ret;
  }

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

packed_error_t _traverse(recursion_info_t rec_inf, cbor_item_t** new_item,
                         cbor_item_t** new_packing_table) {
#if PACKED_ENABLE_DEBUG
  PRINT_DEBUG_MSG("traverse", rec_inf.item, rec_inf.parent.item);
#endif

  switch (cbor_typeof(rec_inf.item)) {
    case CBOR_TYPE_ARRAY: {
      for (size_t i = 0; i < cbor_array_size(rec_inf.item); i++) {
        cbor_item_t* child = cbor_array_get(rec_inf.item, i);
        cbor_item_t* child_new_item = NULL;
        cbor_item_t* child_new_packing_table = NULL;
        packed_error_t ret = _traverse(
            _new_rec_info(rec_inf.tables, child, rec_inf.item, i, false,
                          rec_inf.ref_depth, rec_inf.num_active),
            &child_new_item, &child_new_packing_table);
        if (ret != PACKED_ERR_NONE) {
          return ret;
        }
        cbor_decref(&child);
      }
      return PACKED_ERR_NONE;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(rec_inf.item);
      for (size_t i = 0; i < cbor_map_size(rec_inf.item); i++) {
        cbor_item_t* child_new_item = NULL;
        cbor_item_t* child_new_packing_table = NULL;
        cbor_item_t* key = cbor_incref(pairs[i].key);
        packed_error_t ret =
            _traverse(_new_rec_info(rec_inf.tables, key, rec_inf.item, i, true,
                                    rec_inf.ref_depth, rec_inf.num_active),
                      &child_new_item, &child_new_packing_table);
        if (ret != PACKED_ERR_NONE) {
          cbor_decref(&key);
          return ret;
        }
        cbor_decref(&key);

        child_new_item = NULL;
        child_new_packing_table = NULL;
        cbor_item_t* value = cbor_incref(pairs[i].value);
        ret = _traverse(
            _new_rec_info(rec_inf.tables, value, rec_inf.item, i, false,
                          rec_inf.ref_depth, rec_inf.num_active),
            &child_new_item, &child_new_packing_table);
        if (ret != PACKED_ERR_NONE) {
          cbor_decref(&value);
          return ret;
        }
        cbor_decref(&value);
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
          if (ret != PACKED_ERR_NONE) {
            return ret;
          }
          rec_inf.num_active++;
          if (rec_inf.parent.item == NULL) {
            *new_item = rec_inf.item;
          }

          cbor_item_t* child_new_item = NULL;
          cbor_item_t* child_new_packing_table = NULL;
          /* Restart _traverse from the packed data!
           * Note: rec_inf pointers are already updated, since we passed them
           * directly above.*/
          ret = _traverse(rec_inf, &child_new_item, &child_new_packing_table);
          /* Since _handle_tag_113 gave us ownership of the packing table, we
           * are responisble for feeing it after moving upwards out of its
           * scope */
          cbor_decref(&rec_inf.tables[rec_inf.num_active - 1]);
          rec_inf.num_active--;
          if (ret != PACKED_ERR_NONE) {
            // if (rec_inf.num_active > 0) {
            //   cbor_decref(&rec_inf.tables[rec_inf.num_active - 1]);
            //   rec_inf.num_active--;
            // }
            return ret;
          }
          if (child_new_item != NULL) {
            rec_inf.item = child_new_item;
          }
          if (rec_inf.parent.item == NULL) {
            *new_item = rec_inf.item;
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
        case 135: {
          /* Get rump and argument */
          size_t idx = cbor_tag_value(rec_inf.item) - 128;
          cbor_item_t* argument = NULL;
          packed_error_t ret =
              _packing_table_get(rec_inf.tables, rec_inf.num_active, idx,
                                 &argument, NULL, NULL, NULL);
          if (ret != PACKED_ERR_NONE) {
            return ret;
          }
          cbor_item_t* rump = cbor_tag_item(rec_inf.item);
          if (rump == NULL) {
            return PACKED_ERR_UNEXPECTED_FORMAT;
          }

          /* Unpack both recursively */
          ret = _traverse(
              _new_rec_info(rec_inf.tables, rump, rec_inf.item, 0, false,
                            rec_inf.ref_depth, rec_inf.num_active),
              &rump, &rec_inf.tables[rec_inf.num_active]);
          if (ret != PACKED_ERR_NONE) {
            cbor_decref(&rump);
            return ret;
          }
          ret = _traverse(
              _new_rec_info(rec_inf.tables, argument, rec_inf.item, 0, false,
                            rec_inf.ref_depth, rec_inf.num_active),
              &argument, &rec_inf.tables[rec_inf.num_active]);
          if (ret != PACKED_ERR_NONE) {
            cbor_decref(&argument);
            return ret;
          }
        }

        case 6: {
          cbor_item_t* tag_new_item = NULL;
          packed_error_t ret =
              _handle_tag_6(rec_inf.tables, rec_inf.num_active, rec_inf.parent,
                            rec_inf.item, &tag_new_item);
          if (ret == _NESTED) {
            cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
            cbor_item_t* child_new_item = NULL;
            cbor_item_t* child_new_packing_table = NULL;
            ret = _traverse(
                _new_rec_info(rec_inf.tables, tag_child, rec_inf.item, 0, false,
                              rec_inf.ref_depth, rec_inf.num_active),
                &child_new_item, &child_new_packing_table);
            cbor_decref(&tag_child);
            if (ret != PACKED_ERR_NONE) {
              return ret;
            }

            ret = _handle_tag_6(rec_inf.tables, rec_inf.num_active,
                                rec_inf.parent, rec_inf.item, &tag_new_item);
            if (ret == _NESTED) {
              cbor_item_t* unpacked_child = cbor_tag_item(rec_inf.item);
              if (unpacked_child == NULL) {
                return PACKED_ERR_UNEXPECTED_FORMAT;
              }
              ret = _replace(rec_inf.parent, rec_inf.item, unpacked_child);
              if (ret != PACKED_ERR_NONE) {
                cbor_decref(&unpacked_child);
                return ret;
              }
              rec_inf.item = unpacked_child;
              if (rec_inf.parent.item != NULL) {
                cbor_decref(&unpacked_child);
              }
            }
            if (ret == _RESOLVE_THEN_REPLACE || ret == _NESTED) {
              ret = _traverse(rec_inf, new_item,
                              new_packing_table);  // restart process
              return ret;
            }
            if (ret != PACKED_ERR_NONE) {
              return ret;
            }
            if (tag_new_item != NULL) {
              rec_inf.item = tag_new_item;
            }
            return PACKED_ERR_NONE;
          }
          if (ret == _RESOLVE_THEN_REPLACE) {
            rec_inf.ref_depth++;
            if (rec_inf.ref_depth > MAX_REFERENCE_DEPTH) {
              return PACKED_ERR_MAX_REF_DEPTH_EXCEEDED;
            }

            cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
            size_t index = cbor_get_int(tag_child);
            cbor_item_t* unpacked_item = NULL;
            cbor_item_t* unpacked_item_table = NULL;
            size_t unpacked_item_index = 0;
            uint8_t unpacked_item_num_active = 0;
            ret = _packing_table_get(rec_inf.tables, rec_inf.num_active, index,
                                     &unpacked_item, &unpacked_item_table,
                                     &unpacked_item_index,
                                     &unpacked_item_num_active);
            if (ret != PACKED_ERR_NONE) {
              cbor_decref(&tag_child);
              return ret;
            }

            cbor_item_t* child_new_item = NULL;
            cbor_item_t* child_new_packing_table = NULL;
            ret = _traverse(
                _new_rec_info(rec_inf.tables, unpacked_item,
                              unpacked_item_table, unpacked_item_index, false,
                              rec_inf.ref_depth, unpacked_item_num_active),
                &child_new_item, &child_new_packing_table);
            if (ret != PACKED_ERR_NONE) {
              cbor_decref(&tag_child);
              cbor_decref(&unpacked_item);
              return ret;
            }

            ret = _handle_tag_6(rec_inf.tables, rec_inf.num_active,
                                rec_inf.parent, rec_inf.item, &tag_new_item);
            if (ret != PACKED_ERR_NONE) {
              cbor_decref(&tag_child);
              cbor_decref(&unpacked_item);
              return ret;
            }
            if (tag_new_item != NULL) {
              rec_inf.item = tag_new_item;
            }

            cbor_decref(&tag_child);
            cbor_decref(&unpacked_item);
          }
          if (ret == PACKED_ERR_NONE) {
            if (tag_new_item != NULL) {
              rec_inf.item = tag_new_item;
            }
            if (rec_inf.parent.item == NULL) {
              *new_item = rec_inf.item;
              return PACKED_ERR_NONE;
            }
          }
          return ret;
        }
        default: {
          /* Tag not specified by packed cbor */
          cbor_item_t* tag_child = cbor_tag_item(rec_inf.item);
          cbor_item_t* child_new_item = NULL;
          cbor_item_t* child_new_packing_table = NULL;
          packed_error_t ret = _traverse(
              _new_rec_info(rec_inf.tables, tag_child, rec_inf.item, 0, false,
                            rec_inf.ref_depth, rec_inf.num_active),
              &child_new_item, &child_new_packing_table);
          cbor_decref(&tag_child);
          if (ret != PACKED_ERR_NONE) {
            return ret;
          }
          return PACKED_ERR_NONE;
        }
      }
    }
    default: {
      /* normal leaf node */
      return PACKED_ERR_NONE;
    }
  }
}