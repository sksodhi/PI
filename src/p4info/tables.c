/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PI/p4info/tables.h"
#include "p4info/p4info_struct.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INLINE_MATCH_FIELDS 8
#define INLINE_ACTIONS 8

typedef struct {
  char *name;
  pi_p4_id_t field_id;
  pi_p4info_match_type_t match_type;
  size_t bitwidth;
} _match_field_data_t;

typedef struct _table_data_s {
  char *name;
  pi_p4_id_t table_id;
  size_t num_match_fields;
  size_t num_actions;
  union {
    pi_p4_id_t direct[INLINE_MATCH_FIELDS];
    pi_p4_id_t *indirect;
  } match_field_ids;
  union {
    _match_field_data_t direct[INLINE_MATCH_FIELDS];
    _match_field_data_t *indirect;
  } match_field_data;
  union {
    pi_p4_id_t direct[INLINE_ACTIONS];
    pi_p4_id_t *indirect;
  } action_ids;
  size_t match_fields_added;
  size_t actions_added;
} _table_data_t;

static size_t get_table_idx(pi_p4_id_t table_id) {
  return table_id & 0xFFFF;
}

static _table_data_t *get_table(const pi_p4info_t *p4info,
                                pi_p4_id_t table_id) {
  size_t table_idx = get_table_idx(table_id);
  assert(table_idx < p4info->num_tables);
  return &p4info->tables[table_idx];
}

static pi_p4_id_t *get_match_field_ids(_table_data_t *table) {
  return (table->num_match_fields <= INLINE_MATCH_FIELDS) ?
      table->match_field_ids.direct : table->match_field_ids.indirect;
}

static _match_field_data_t *get_match_field_data(_table_data_t *table) {
  return (table->num_match_fields <= INLINE_MATCH_FIELDS) ?
      table->match_field_data.direct : table->match_field_data.indirect;
}

static pi_p4_id_t *get_action_ids(_table_data_t *table) {
  return (table->num_actions <= INLINE_ACTIONS) ?
      table->action_ids.direct : table->action_ids.indirect;
}

void pi_p4info_table_init(pi_p4info_t *p4info, size_t num_tables) {
  p4info->num_tables = num_tables;
  p4info->tables = calloc(num_tables, sizeof(_table_data_t));
  p4info->table_name_map = (Pvoid_t) NULL;
}

void pi_p4info_table_free(pi_p4info_t *p4info) {
  for (size_t i = 0; i < p4info->num_tables; i++) {
    _table_data_t *table = &p4info->tables[i];
    if (!table->name) continue;
    free(table->name);
    _match_field_data_t *match_fields = get_match_field_data(table);
    for (size_t j = 0; j < table->num_match_fields; j++) {
      _match_field_data_t *match_field = &match_fields[j];
      if (!match_field->name) continue;
      free(match_field->name);
    }
    if (table->num_match_fields > INLINE_MATCH_FIELDS) {
      assert(table->match_field_ids.indirect);
      assert(table->match_field_data.indirect);
      free(table->match_field_ids.indirect);
      free(table->match_field_data.indirect);
    }
    if (table->num_actions > INLINE_ACTIONS) {
      assert(table->action_ids.indirect);
      free(table->action_ids.indirect);
    }
  }
  free(p4info->tables);
  Word_t Rc_word;
  JSLFA(Rc_word, p4info->table_name_map);
}

void pi_p4info_table_add(pi_p4info_t *p4info, pi_p4_id_t table_id,
                         const char *name, size_t num_match_fields,
                         size_t num_actions) {
  _table_data_t *table = get_table(p4info, table_id);
  table->name = strdup(name);
  table->table_id = table_id;
  table->num_match_fields = num_match_fields;
  table->num_actions = num_actions;
  if (num_match_fields > INLINE_MATCH_FIELDS) {
    table->match_field_ids.indirect =
        calloc(num_match_fields, sizeof(pi_p4_id_t));
    table->match_field_data.indirect =
        calloc(num_match_fields, sizeof(_match_field_data_t));
  }
  if (num_actions > INLINE_ACTIONS) {
    table->action_ids.indirect = calloc(num_actions, sizeof(pi_p4_id_t));
  }

  Word_t *table_id_ptr;
  JSLI(table_id_ptr, p4info->table_name_map, (const uint8_t *) table->name);
  *table_id_ptr = table_id;
}

void pi_p4info_table_add_match_field(pi_p4info_t *p4info, pi_p4_id_t table_id,
                                     pi_p4_id_t field_id, const char *name,
                                     pi_p4info_match_type_t match_type,
                                     size_t bitwidth) {
  _table_data_t *table = get_table(p4info, table_id);
  assert(table->match_fields_added < table->num_match_fields);
  _match_field_data_t *match_field =
      &get_match_field_data(table)[table->match_fields_added];
  assert(!match_field->name);
  match_field->name = strdup(name);
  match_field->field_id = field_id;
  match_field->match_type = match_type;
  match_field->bitwidth = bitwidth;
  get_match_field_ids(table)[table->match_fields_added] = field_id;
  table->match_fields_added++;
}

void pi_p4info_table_add_action(pi_p4info_t *p4info, pi_p4_id_t table_id,
                                pi_p4_id_t action_id) {
  _table_data_t *table = get_table(p4info, table_id);
  assert(table->actions_added < table->num_actions);
  get_action_ids(table)[table->actions_added] = action_id;
  table->actions_added++;
}

pi_p4_id_t pi_p4info_table_id_from_name(const pi_p4info_t *p4info,
                                        const char *name) {
  Word_t *table_id_ptr;
  JSLG(table_id_ptr, p4info->table_name_map, (const uint8_t *) name);
  assert (table_id_ptr);
  return *table_id_ptr;
}

const char *pi_p4info_table_name_from_id(const pi_p4info_t *p4info,
                                         pi_p4_id_t table_id) {
  _table_data_t *table = get_table(p4info, table_id);
  return table->name;
}

size_t pi_p4info_table_num_match_fields(const pi_p4info_t *p4info,
                                        pi_p4_id_t table_id) {
  _table_data_t *table = get_table(p4info, table_id);
  return table->num_match_fields;
}

const pi_p4_id_t *pi_p4info_table_get_match_fields(const pi_p4info_t *p4info,
                                                   pi_p4_id_t table_id,
                                                   size_t *num_match_fields) {
  _table_data_t *table = get_table(p4info, table_id);
  *num_match_fields = table->num_match_fields;
  return get_match_field_ids(table);
}

bool pi_p4info_table_is_match_field_of(const pi_p4info_t *p4info,
                                       pi_p4_id_t table_id,
                                       pi_p4_id_t field_id) {
  _table_data_t *table = get_table(p4info, table_id);
  pi_p4_id_t *ids = get_match_field_ids(table);
  for (size_t i = 0; i < table->num_match_fields; i++)
    if (ids[i] == field_id) return true;
  return false;
}

size_t pi_p4info_table_match_field_index(const pi_p4info_t *p4info,
                                         pi_p4_id_t table_id,
                                         pi_p4_id_t field_id) {
  _table_data_t *table = get_table(p4info, table_id);
  pi_p4_id_t *ids = get_match_field_ids(table);
  for (size_t i = 0; i < table->num_match_fields; i++)
    if (ids[i] == field_id) return i;
  return (size_t) -1;
}

void pi_p4info_table_match_field_info(const pi_p4info_t *p4info,
                                      pi_p4_id_t table_id,
                                      size_t index,
                                      pi_p4info_match_field_info_t *info) {
  _table_data_t *table = get_table(p4info, table_id);
  _match_field_data_t *data = &get_match_field_data(table)[index];
  info->name = data->name;
  info->field_id = data->field_id;
  info->match_type = data->match_type;
  info->bitwidth = data->bitwidth;
}

size_t pi_p4info_table_num_actions(const pi_p4info_t *p4info,
                                   pi_p4_id_t table_id) {
  _table_data_t *table = get_table(p4info, table_id);
  return table->num_actions;
}

bool pi_p4info_table_is_action_of(const pi_p4info_t *p4info,
                                  pi_p4_id_t table_id,
                                  pi_p4_id_t action_id) {
  _table_data_t *table = get_table(p4info, table_id);
  pi_p4_id_t *ids = get_action_ids(table);
  for (size_t i = 0; i < table->num_actions; i++)
    if (ids[i] == action_id) return true;
  return false;
}

const pi_p4_id_t *pi_p4info_table_get_actions(const pi_p4info_t *p4info,
                                              pi_p4_id_t table_id,
                                              size_t *num_actions) {
  _table_data_t *table = get_table(p4info, table_id);
  *num_actions = table->num_actions;
  return get_action_ids(table);
}
