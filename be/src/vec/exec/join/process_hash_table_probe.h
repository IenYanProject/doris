// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <vector>

#include "join_op.h"
#include "vec/columns/column.h"
#include "vec/columns/columns_number.h"
#include "vec/common/arena.h"

namespace doris {
namespace vectorized {

class Block;
class MutableBlock;
struct HashJoinProbeContext;

using MutableColumnPtr = IColumn::MutablePtr;
using MutableColumns = std::vector<MutableColumnPtr>;

using NullMap = ColumnUInt8::Container;
using ConstNullMapPtr = const NullMap*;

template <int JoinOpType>
struct ProcessHashTableProbe {
    ProcessHashTableProbe(HashJoinProbeContext* join_context, int batch_size);
    ~ProcessHashTableProbe() = default;

    // output build side result column
    template <bool have_other_join_conjunct = false>
    void build_side_output_column(MutableColumns& mcol, int column_offset, int column_length,
                                  const std::vector<bool>& output_slot_flags, int size);

    void probe_side_output_column(MutableColumns& mcol, const std::vector<bool>& output_slot_flags,
                                  int size, int last_probe_index, size_t probe_size,
                                  bool all_match_one, bool have_other_join_conjunct);
    // Only process the join with no other join conjunct, because of no other join conjunt
    // the output block struct is same with mutable block. we can do more opt on it and simplify
    // the logic of probe
    // TODO: opt the visited here to reduce the size of hash table
    template <bool need_null_map_for_probe, bool ignore_null, typename HashTableType>
    Status do_process(HashTableType& hash_table_ctx, ConstNullMapPtr null_map,
                      MutableBlock& mutable_block, Block* output_block, size_t probe_rows,
                      bool is_mark_join);
    // In the presence of other join conjunct, the process of join become more complicated.
    // each matching join column need to be processed by other join conjunct. so the struct of mutable block
    // and output block may be different
    // The output result is determined by the other join conjunct result and same_to_prev struct
    template <bool need_null_map_for_probe, bool ignore_null, typename HashTableType>
    Status do_process_with_other_join_conjuncts(HashTableType& hash_table_ctx,
                                                ConstNullMapPtr null_map,
                                                MutableBlock& mutable_block, Block* output_block,
                                                size_t probe_rows, bool is_mark_join);

    void _process_splited_equal_matched_tuples(int start_row_idx, int row_count,
                                               const ColumnPtr& other_hit_column,
                                               std::vector<bool*>& visited_map, int right_col_idx,
                                               int right_col_len, UInt8* __restrict null_map_data,
                                               UInt8* __restrict filter_map, Block* output_block);

    void _pre_serialize_key(const ColumnRawPtrs& key_columns, const size_t key_rows,
                            std::vector<StringRef>& serialized_keys);

    // Process full outer join/ right join / right semi/anti join to output the join result
    // in hash table
    template <typename HashTableType>
    Status process_data_in_hashtable(HashTableType& hash_table_ctx, MutableBlock& mutable_block,
                                     Block* output_block, bool* eos);

    vectorized::HashJoinProbeContext* _join_context;
    const int _batch_size;
    const std::vector<Block>& _build_blocks;
    std::unique_ptr<Arena> _arena;
    std::vector<StringRef> _probe_keys;

    std::vector<uint32_t> _probe_indexs;
    std::vector<int8_t> _build_block_offsets;
    std::vector<int> _build_block_rows;
    std::vector<std::pair<int8_t, int>> _build_blocks_locs;
    // only need set the tuple is null in RIGHT_OUTER_JOIN and FULL_OUTER_JOIN
    ColumnUInt8::Container* _tuple_is_null_left_flags;
    // only need set the tuple is null in LEFT_OUTER_JOIN and FULL_OUTER_JOIN
    ColumnUInt8::Container* _tuple_is_null_right_flags;

    size_t _serialized_key_buffer_size {0};
    uint8_t* _serialized_key_buffer;
    std::unique_ptr<Arena> _serialize_key_arena;
    std::vector<size_t> _probe_side_hash_values;

    RuntimeProfile::Counter* _rows_returned_counter;
    RuntimeProfile::Counter* _search_hashtable_timer;
    RuntimeProfile::Counter* _build_side_output_timer;
    RuntimeProfile::Counter* _probe_side_output_timer;
    RuntimeProfile::Counter* _probe_process_hashtable_timer;
    static constexpr int PROBE_SIDE_EXPLODE_RATE = 3;
};

} // namespace vectorized
} // namespace doris
