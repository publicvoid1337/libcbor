#include "packed.h"
#include <cbor/arrays.h>
#include <cbor/common.h>
#include <cbor/data.h>
#include <cbor/strings.h>
#include <cbor/tags.h>
#include <stddef.h>
#include <stdint.h>
#include "arrays.h"
#include "ints.h"

const char* describe_error(packed_error_t error) {
  switch (error) {
    case PACKED_ERR_NONE:
      return "PACKED_ERR_NONE";
    case PACKED_ERR_UNDEFINED_REFERENCE:
      return "PACKED_ERR_UNDEFINED_REFERENCE";
    case PACKED_ERR_OUT_OF_BOUNDS:
      return "PACKED_ERR_OUT_OF_BOUNDS";
    case PACKED_ERR_UNEXPECTED_FORMAT:
      return "PACKED_ERR_UNEXPECTED_FORMAT";
    case PACKED_ERR_NOT_SUPPORTED:
      return "PACKED_ERR_NOT_SUPPORTED";
    case PACKED_ERR_MAX_REF_DEPTH_EXCEEDED:
      return "PACKED_ERR_MAX_REF_DEPTH_EXCEEDED";
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

#if PACKED_ENABLE_DEBUG
static int ctr = 0;

void _print_dbg(bool is_return, const char* fn_name, cbor_item_t* curr,
                packed_error_t err) {
  if (is_return) {
    ctr--;
    printf("\x1b[38;5;%dm %*s[<-][%s][%s][%s] \x1b[0m\n", ctr, 2 * ctr, "",
           fn_name, describe_cbor_type(curr), describe_error(err));
  } else {
    printf("\x1b[38;5;%dm %*s[->][%s][%s] \x1b[0m\n", ctr, 2 * ctr, "", fn_name,
           describe_cbor_type(curr));
    ctr++;
  }
}
#endif

//
//
//

packed_error_t _concatenate(cbor_item_t* lhs, cbor_item_t* rhs,
                            cbor_item_t** out, cbor_type string_out_type) {
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

packed_error_t _splice(cbor_item_t* outer_arr, cbor_item_t* splicee, size_t idx,
                       cbor_item_t** out) {
  if (outer_arr == NULL || cbor_typeof(outer_arr) != CBOR_TYPE_ARRAY ||
      splicee == NULL || cbor_typeof(splicee) != CBOR_TYPE_ARRAY) {
    return PACKED_ERR_UNEXPECTED_FORMAT;
  }
  size_t arr_len = cbor_array_size(outer_arr);
  cbor_item_t** arr_handle = cbor_array_handle(outer_arr);
  size_t splicee_len = cbor_array_size(splicee);
  cbor_item_t** splicee_handle = cbor_array_handle(splicee);
  cbor_item_t* res = cbor_new_definite_array(arr_len + splicee_len);
  cbor_item_t** res_handle = cbor_array_handle(res);

  for (size_t i = 0; i < arr_len; i++) {
    if (i == idx) {
      for (size_t j = 0; j < splicee_len; j++) {
        cbor_array_push(res, splicee_handle[j]);
      }
    } else {
      cbor_array_push(res, arr_handle[i]);
    }
  }

  *out = res;
  return PACKED_ERR_NONE;
}

packed_error_t _replace(parent_t parent, cbor_item_t* old_item,
                        cbor_item_t* new_item) {
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

#define _NEO_TRAVERSE_2(rec_inf, replace_ctx, old_item)                \
  do {                                                                 \
    neo_tabledef_t* starting_table = rec_inf.tabledef;                 \
    packed_response_t resp;                                            \
                                                                       \
    do {                                                               \
      resp = _neo_traverse(rec_inf);                                   \
      if (resp.err != PACKED_ERR_NONE) {                               \
        break;                                                         \
      }                                                                \
                                                                       \
      if (resp.flags.replace_child) {                                  \
        assert(_replace(replace_ctx, old_item, resp.data.new_child) == \
               PACKED_ERR_NONE);                                       \
        cbor_decref(&resp.data.new_child);                             \
        rec_inf.curr = resp.data.new_child;                            \
      }                                                                \
      if (resp.flags.new_tabledef) {                                   \
        neo_tabledef_t* new_tabledef = malloc(sizeof(neo_tabledef_t)); \
        *new_tabledef = resp.data.new_table;                           \
        new_tabledef->prev = rec_inf.tabledef;                         \
        rec_inf.tabledef = new_tabledef;                               \
      }                                                                \
      if (resp.flags.increase_depth) {                                 \
        rec_inf.depth++;                                               \
      }                                                                \
    } while (resp.flags.replace_child);                                \
                                                                       \
    while (rec_inf.tabledef != starting_table) {                       \
      cbor_decref(&rec_inf.tabledef->data.combined_table);             \
      neo_tabledef_t* next = rec_inf.tabledef->prev;                   \
      free(rec_inf.tabledef);                                          \
      rec_inf.tabledef = next;                                         \
    }                                                                  \
                                                                       \
    if (resp.err != PACKED_ERR_NONE) {                                 \
      cbor_decref(&rec_inf.curr);                                      \
      return resp;                                                     \
    }                                                                  \
  } while (0)

packed_error_t _neo_tabledef_get(neo_tabledef_t* table, size_t idx,
                                 cbor_item_t** out) {
  if (table->is_basic) {
    while (table != NULL) {
      size_t table_len = cbor_array_size(table->data.combined_table);
      if (idx < table_len) {
        *out = cbor_copy(cbor_array_handle(table->data.combined_table)[idx]);
        return PACKED_ERR_NONE;
      }
      idx -= table_len;
      table = table->prev;
    }
    return PACKED_ERR_UNDEFINED_REFERENCE;
  } else {
    return PACKED_ERR_NOT_SUPPORTED;
  }
}

packed_response_t _handle_shared_item_ref(neo_rec_inf_t* rec_inf, size_t idx) {
  packed_response_t resp = {0};

  if (rec_inf->depth >= MAX_REFERENCE_DEPTH) {
    resp.err = PACKED_ERR_MAX_REF_DEPTH_EXCEEDED;
#if PACKED_ENABLE_DEBUG
    _print_dbg(true, "handle_shared_ref", rec_inf->curr,
               PACKED_ERR_MAX_REF_DEPTH_EXCEEDED);
#endif
    return resp;
  }

  packed_error_t ret =
      _neo_tabledef_get(rec_inf->tabledef, idx, &resp.data.new_child);
  if (ret != PACKED_ERR_NONE) {
    resp.err = ret;
#if PACKED_ENABLE_DEBUG
    _print_dbg(true, "handle_shared_ref", rec_inf->curr, ret);
#endif
    return resp;
  }

  resp.err = PACKED_ERR_NONE;
  resp.flags.increase_depth = true;
  resp.flags.replace_child = true;
#if PACKED_ENABLE_DEBUG
  _print_dbg(true, "handle_shared_ref", resp.data.new_child, PACKED_ERR_NONE);
#endif
  return resp;
}

packed_response_t _handle_arg_ref(neo_rec_inf_t* rec_inf, size_t idx,
                                  bool is_straight) {
  packed_response_t resp = {0};

  /* Get argument and rump*/
  cbor_item_t* argument = NULL;
  packed_error_t ret = _neo_tabledef_get(rec_inf->tabledef, idx, &argument);
  if (ret != PACKED_ERR_NONE) {
    resp.err = ret;
#if PACKED_ENABLE_DEBUG
    _print_dbg(true, "traverse", rec_inf->curr, ret);
#endif
    return resp;
  }

  cbor_item_t* rump = cbor_tag_item(rec_inf->curr);
  if (rump == NULL) {
    cbor_decref(&argument);
    resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
    _print_dbg(true, "traverse", rec_inf->curr, PACKED_ERR_UNEXPECTED_FORMAT);
#endif
    return resp;
  }

  /* Determine lhs and rhs as well as function to be applied */
  cbor_item_t* lhs = is_straight ? argument : rump;
  cbor_item_t* rhs = is_straight ? rump : argument;
  cbor_item_t* function_argument = NULL;

  int function_id = 0;
  if (cbor_typeof(lhs) == CBOR_TYPE_TAG) {
    function_id = cbor_tag_value(lhs);
    // we do not modify the function tag itself
    function_argument = cbor_tag_item(lhs);
    if (function_argument == NULL) {
      cbor_decref(&argument);
      cbor_decref(&rump);
      resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
      _print_dbg(true, "traverse", rec_inf->curr, PACKED_ERR_UNEXPECTED_FORMAT);
#endif
      return resp;
    }
    lhs = function_argument;
  }

  /* Unpack both recursively */
  rec_inf->curr = rump;
  parent_t replace_ctx = {.item = rec_inf->curr};
  _NEO_TRAVERSE_2((*rec_inf), replace_ctx, rump);

  rec_inf->curr = argument;
  cbor_item_t* wrapper = cbor_new_definite_array(1);
  cbor_array_handle(wrapper)[0] = argument;
  replace_ctx.item = wrapper;
  replace_ctx.index = 0;
  _NEO_TRAVERSE_2((*rec_inf), replace_ctx, argument);
  cbor_decref(&wrapper);

  /* Apply function */
  cbor_item_t* res = NULL;
  switch (function_id) {
    /* No function tag specified - concatenate */
    case 0: {
      ret = _concatenate(lhs, rhs, &res, cbor_typeof(rump));
      CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument, res);
      break;
    }
    /* Interchanged join */
    case 105: {
      ret = _join(rhs, lhs, &res);
      CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument, res);
      break;
    }
    /* Join*/
    case 106: {
      ret = _join(lhs, rhs, &res);
      CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument, res);
      break;
    }
    /* Record */
    case 114: {
      ret = _record(lhs, rhs, &res);
      CATCH_DECREF_RETURN_1(ret, argument, rump, function_argument, res);
      break;
    }
  }

  /* Replace reference with result of function application */
  if (function_argument != NULL) {
    cbor_decref(&function_argument);
  }
  cbor_decref(&argument);
  cbor_decref(&rump);
  resp.flags.replace_child = true;
  resp.data.new_child = res;
#if PACKED_ENABLE_DEBUG
  _print_dbg(true, "traverse", res, PACKED_ERR_NONE);
#endif
  return resp;
}

packed_response_t _neo_traverse(neo_rec_inf_t rec_inf) {
#if PACKED_ENABLE_DEBUG
  _print_dbg(false, "traverse", rec_inf.curr, PACKED_ERR_NONE);
#endif
  switch (cbor_typeof(rec_inf.curr)) {
    case CBOR_TYPE_ARRAY: {
      packed_response_t resp = {0};

      bool was_spliced = false;
      cbor_item_t** handle = cbor_array_handle(rec_inf.curr);
      for (size_t i = 0; i < cbor_array_size(rec_inf.curr); i++) {
        neo_rec_inf_t next_rec_inf = {.curr = handle[i],
                                      .depth = rec_inf.depth,
                                      .tabledef = rec_inf.tabledef};
        parent_t replace_ctx = {.item = rec_inf.curr, .index = i};
        _NEO_TRAVERSE_2(next_rec_inf, replace_ctx, handle[i]);

        if (cbor_typeof(handle[i]) == CBOR_TYPE_TAG &&
            cbor_tag_value(handle[i]) == 1115) {
          cbor_item_t* res = NULL;
          cbor_item_t* splicee = cbor_tag_item(handle[i]);
          packed_error_t ret = _splice(rec_inf.curr, splicee, i, &res);
          cbor_decref(&handle[i]);
          if (ret != PACKED_ERR_NONE) {
            resp.err = ret;
#if PACKED_ENABLE_DEBUG
            _print_dbg(true, "traverse", rec_inf.curr, ret);
#endif
            return resp;
          }

          rec_inf.curr = res;
          handle = cbor_array_handle(rec_inf.curr);
          was_spliced = true;
          i--; /* Redo traverse call on spliced array */
        }
      }

      if (was_spliced) {
        resp.flags.replace_child = true;
        resp.data.new_child = rec_inf.curr;
      }
      resp.err = PACKED_ERR_NONE;
#if PACKED_ENABLE_DEBUG
      _print_dbg(true, "traverse", rec_inf.curr, PACKED_ERR_NONE);
#endif
      return resp;
    }
    case CBOR_TYPE_MAP: {
      struct cbor_pair* pairs = cbor_map_handle(rec_inf.curr);
      for (size_t i = 0; i < cbor_map_size(rec_inf.curr); i++) {
        neo_rec_inf_t next_rec_inf = {.curr = pairs[i].key,
                                      .depth = rec_inf.depth,
                                      .tabledef = rec_inf.tabledef};
        parent_t replace_ctx = {
            .item = rec_inf.curr, .index = i, .is_key = true};
        _NEO_TRAVERSE_2(next_rec_inf, replace_ctx, pairs[i].key);

        replace_ctx.is_key = false;
        next_rec_inf.curr = pairs[i].value;
        _NEO_TRAVERSE_2(next_rec_inf, replace_ctx, pairs[i].value);
      }

      packed_response_t resp = {0};
      resp.err = PACKED_ERR_NONE;
#if PACKED_ENABLE_DEBUG
      _print_dbg(true, "traverse", rec_inf.curr, PACKED_ERR_NONE);
#endif
      return resp;
    }
    case CBOR_TYPE_FLOAT_CTRL: {
      packed_response_t resp = {0};

      if (cbor_float_ctrl_is_ctrl(rec_inf.curr) &&
          cbor_ctrl_value(rec_inf.curr) < 16) {
        return _handle_shared_item_ref(&rec_inf, cbor_ctrl_value(rec_inf.curr));
      }

      resp.err = PACKED_ERR_NONE;
#if PACKED_ENABLE_DEBUG
      _print_dbg(true, "traverse", rec_inf.curr, PACKED_ERR_NONE);
#endif
      return resp;
    }
    case CBOR_TYPE_TAG: {
      packed_response_t resp = {0};

      switch (cbor_tag_value(rec_inf.curr)) {
        case 6: {
          packed_response_t resp = {0};

          cbor_item_t* child = cbor_tag_item(rec_inf.curr);
        x:
          if (child == NULL) {
            resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
            _print_dbg(true, "traverse", rec_inf.curr,
                       PACKED_ERR_UNEXPECTED_FORMAT);
#endif
            return resp;
          }

          if (cbor_typeof(child) == CBOR_TYPE_UINT ||
              cbor_typeof(child) == CBOR_TYPE_NEGINT) {
            // TODO: correct index calculation
            size_t idx = cbor_get_int(child);
            cbor_decref(&child);

            return _handle_shared_item_ref(&rec_inf, idx);
          } else {
            cbor_item_t* _child = child;
            neo_rec_inf_t next_rec_inf = {.curr = child,
                                          .depth = rec_inf.depth,
                                          .tabledef = rec_inf.tabledef};
            parent_t replace_ctx = {.item = rec_inf.curr};
            _NEO_TRAVERSE_2(next_rec_inf, replace_ctx, child);

            cbor_decref(&child);
            return _neo_traverse(rec_inf);
          }
        }
        case 113: {
          cbor_item_t* enclosing_arr = cbor_tag_item(rec_inf.curr);
          if (enclosing_arr == NULL || cbor_array_size(enclosing_arr) != 2) {
            resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
            _print_dbg(true, "traverse", rec_inf.curr,
                       PACKED_ERR_UNEXPECTED_FORMAT);
#endif
            return resp;
          }

          cbor_item_t* table = cbor_array_get(enclosing_arr, 0);
          if (table == NULL || cbor_typeof(table) != CBOR_TYPE_ARRAY) {
            resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
            _print_dbg(true, "traverse", rec_inf.curr,
                       PACKED_ERR_UNEXPECTED_FORMAT);
#endif
            return resp;
          }

          resp.flags.new_tabledef = true;
          if (rec_inf.tabledef == NULL) {
            neo_tabledef_t tabledef = {
                .data.combined_table = table, .is_basic = true, .prev = NULL};
            resp.data.new_table = tabledef;
          } else {
            neo_tabledef_t tabledef = {.data.combined_table = table,
                                       .is_basic = true,
                                       .prev = rec_inf.tabledef};
            resp.data.new_table = tabledef;
          }

          cbor_item_t* packed_data = cbor_array_get(enclosing_arr, 1);
          if (packed_data == NULL) {
            resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
            _print_dbg(true, "traverse", rec_inf.curr,
                       PACKED_ERR_UNEXPECTED_FORMAT);
#endif
            return resp;
          }
          resp.flags.replace_child = true;
          resp.data.new_child = packed_data;

          cbor_decref(&enclosing_arr);
          resp.err = PACKED_ERR_NONE;
#if PACKED_ENABLE_DEBUG
          _print_dbg(true, "traverse", rec_inf.curr, PACKED_ERR_NONE);
#endif
          return resp;
        }
        /* Argument refrence */
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
          if (cbor_tag_value(rec_inf.curr) < 136) {
            idx = cbor_tag_value(rec_inf.curr) - 128;
            is_straight = true;
          } else {
            idx = cbor_tag_value(rec_inf.curr) - 136;
            is_straight = false;
          }

          return _handle_arg_ref(&rec_inf, idx, is_straight);
        }
        case 1115: {
          cbor_item_t* arg = cbor_tag_item(rec_inf.curr);
          if (arg == NULL || cbor_typeof(arg) != CBOR_TYPE_ARRAY) {
            resp.err = PACKED_ERR_UNEXPECTED_FORMAT;
#if PACKED_ENABLE_DEBUG
            _print_dbg(true, "traverse", rec_inf.curr,
                       PACKED_ERR_UNEXPECTED_FORMAT);
#endif
            return resp;
          }
        }
      }
    }
    default: {
      /* Normal leaf node */
      packed_response_t resp = {0};
      resp.err = PACKED_ERR_NONE;
#if PACKED_ENABLE_DEBUG
      _print_dbg(true, "traverse", rec_inf.curr, PACKED_ERR_NONE);
#endif
      return resp;
    }
  }
}

cbor_item_t* cbor_unpack(cbor_item_t* target,
                         neo_tabledef_t* global_packing_table) {
  neo_rec_inf_t rec_inf = {
      .curr = target, .depth = 0, .tabledef = global_packing_table};
  packed_response_t resp;

  do {
    resp = _neo_traverse(rec_inf);
    if (resp.err != PACKED_ERR_NONE) {
      break;
    }

    if (resp.flags.new_tabledef) {
      neo_tabledef_t* new_tabledef = malloc(sizeof(neo_tabledef_t));
      *new_tabledef = resp.data.new_table;
      new_tabledef->prev = rec_inf.tabledef;
      rec_inf.tabledef = new_tabledef;
    }
    if (resp.flags.replace_child) {
      cbor_decref(&rec_inf.curr);
      rec_inf.curr = resp.data.new_child;
    }
    if (resp.flags.increase_depth) {
      rec_inf.depth++;
    }
  } while (resp.flags.replace_child);

  while (rec_inf.tabledef != global_packing_table) {
    cbor_decref(&rec_inf.tabledef->data.combined_table);
    neo_tabledef_t* next = rec_inf.tabledef->prev;
    free(rec_inf.tabledef);
    rec_inf.tabledef = next;
  }

  if (resp.err != PACKED_ERR_NONE) {
    cbor_decref(&rec_inf.curr);
    return NULL;
  }

  return rec_inf.curr;
}