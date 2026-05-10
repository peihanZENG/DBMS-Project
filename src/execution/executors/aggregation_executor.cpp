#include "onebase/execution/executors/aggregation_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                          std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void AggregationExecutor::Init() {
  child_executor_->Init();
  result_tuples_.clear();
  cursor_ = 0;

  struct AggState {
    std::vector<Value> group_bys;
    std::vector<Value> aggregates;
  };
  std::unordered_map<std::string, AggState> aht;
  
  Tuple child_tuple;
  RID child_rid;
  
  // ✨ 修复 2：加一个标志位，记录我们到底有没有从子节点读到哪怕一行数据
  bool has_tuples = false; 

  const auto &agg_types = plan_->GetAggregateTypes();

  // =================================================================
  // 1. 遍历子节点，构建哈希表
  // =================================================================
  while (child_executor_->Next(&child_tuple, &child_rid)) {
    has_tuples = true; 

    // --- 提取 Group By 列构建 Key ---
    std::string hash_key = "";
    std::vector<Value> group_values;
    for (const auto &expr : plan_->GetGroupBys()) {
      Value val = expr->Evaluate(&child_tuple, &child_executor_->GetOutputSchema());
      group_values.push_back(val);
      hash_key += val.ToString() + "#";
    }
    if (hash_key.empty()) { hash_key = "SINGLE_GROUP"; }

    // --- 提取当前行要聚合的值 ---
    std::vector<Value> agg_values;
    for (const auto &expr : plan_->GetAggregates()) {
      agg_values.push_back(expr->Evaluate(&child_tuple, &child_executor_->GetOutputSchema()));
    }

    // ✨ 修复 1：第一次遇到这个 Key，先铺垫一个“绝对干净”的初始状态
    if (aht.find(hash_key) == aht.end()) {
      AggState initial_state;
      initial_state.group_bys = group_values;
      for (auto agg_type : agg_types) {
        if (agg_type == AggregationType::CountStarAggregate || agg_type == AggregationType::CountAggregate) {
          initial_state.aggregates.push_back(Value(TypeId::INTEGER, 0)); // COUNT 初始为 0
        } else {
          initial_state.aggregates.push_back(Value()); // SUM/MIN/MAX 初始为 NULL
        }
      }
      aht[hash_key] = std::move(initial_state);
    }

    // --- 统一更新逻辑：无论是第 1 行还是第 100 行，都走这里 ---
    auto &state = aht[hash_key];
    for (size_t i = 0; i < agg_types.size(); ++i) {
      auto &current_val = state.aggregates[i];
      const auto &input_val = agg_values[i];

      switch (agg_types[i]) {
        case AggregationType::CountStarAggregate:
          current_val = current_val.Add(Value(TypeId::INTEGER, 1));
          break;

        case AggregationType::CountAggregate:
          if (!input_val.IsNull()) {
            current_val = current_val.Add(Value(TypeId::INTEGER, 1));
          }
          break;

        case AggregationType::SumAggregate:
          if (!input_val.IsNull()) {
            // ✨ 修复 1 附属：如果当前是 NULL，直接赋值；如果有值了，再累加
            if (current_val.IsNull()) {
              current_val = input_val;
            } else {
              current_val = current_val.Add(input_val);
            }
          }
          break;

        case AggregationType::MinAggregate:
          if (!input_val.IsNull() && (current_val.IsNull() || input_val.CompareLessThan(current_val).GetAsBoolean())) {
            current_val = input_val;
          }
          break;

        case AggregationType::MaxAggregate:
          if (!input_val.IsNull() && (current_val.IsNull() || input_val.CompareGreaterThan(current_val).GetAsBoolean())) {
            current_val = input_val;
          }
          break;
      }
    }
  }

  // =================================================================
  // ✨ 修复 2 附属：处理最恶心的“空表全局聚合”情况 (Empty Table)
  // 如果整张表是空的，且没有 GROUP BY 子句 (比如 SELECT COUNT(*))
  // 我们必须手动向结果集中塞入一行 [0, NULL, ...] 的保底数据
  // =================================================================
  if (!has_tuples && plan_->GetGroupBys().empty()) {
    AggState empty_state;
    for (auto agg_type : agg_types) {
      if (agg_type == AggregationType::CountStarAggregate || agg_type == AggregationType::CountAggregate) {
        empty_state.aggregates.push_back(Value(TypeId::INTEGER, 0));
      } else {
        empty_state.aggregates.push_back(Value()); // SUM/MIN/MAX 默认就是 NULL
      }
    }
    aht["SINGLE_GROUP"] = std::move(empty_state);
  }

  // =================================================================
  // 2. 将哈希表转化为 Tuple 输出列表
  // =================================================================
  for (const auto &[key, state] : aht) {
    std::vector<Value> out_values;
    out_values.insert(out_values.end(), state.group_bys.begin(), state.group_bys.end());
    out_values.insert(out_values.end(), state.aggregates.begin(), state.aggregates.end());
    result_tuples_.emplace_back(std::move(out_values));
  }
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Return next aggregation result
  if (cursor_ < result_tuples_.size()) {
    *tuple = result_tuples_[cursor_];
    *rid = RID(INVALID_PAGE_ID, 0); 
    cursor_++;
    return true;
  }
  return false;
}

}  // namespace onebase
