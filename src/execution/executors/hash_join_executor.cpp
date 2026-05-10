#include "onebase/execution/executors/hash_join_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

HashJoinExecutor::HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                                    std::unique_ptr<AbstractExecutor> left_executor,
                                    std::unique_ptr<AbstractExecutor> right_executor)
    : AbstractExecutor(exec_ctx), plan_(plan),
      left_executor_(std::move(left_executor)), right_executor_(std::move(right_executor)) {}

void HashJoinExecutor::Init() {
// TODO(student): Build hash table from right child, initialize left child
  left_executor_->Init();
  right_executor_->Init();
  hash_table_.clear();
  result_tuples_.clear();
  cursor_=0;

  // =================================================================
  // - Phase 1 (in Init): Build hash table from RIGHT child on right_key_expr
  // =================================================================
  Tuple right_tuple;
  RID right_rid;
  // ✨ 修复笔误：这里必须用 right_executor_
  while (right_executor_->Next(&right_tuple, &right_rid)) 
  {
    const auto &right_expr = plan_->GetRightKeyExpression();
    Value right_key_val = right_expr->Evaluate(&right_tuple, &right_executor_->GetOutputSchema());
    std::string hash_key = right_key_val.ToString();
    
    // 把右表的数据塞进哈希表
    hash_table_[hash_key].push_back(right_tuple); 
  }

  // =================================================================
  // - Phase 2 (in Next): For each LEFT tuple, probe hash table using left_key_expr
  // =================================================================
  Tuple left_tuple;
  RID left_rid;
  while (left_executor_->Next(&left_tuple, &left_rid)) 
  {
    const auto &left_expr = plan_->GetLeftKeyExpression();
    Value left_key_val = left_expr->Evaluate(&left_tuple, &left_executor_->GetOutputSchema());
    std::string hash_key = left_key_val.ToString();
    
    if (hash_table_.find(hash_key) != hash_table_.end()) {
      // 找到了匹配！遍历哈希表里所有匹配的右表数据
      for (const auto &matched_right_tuple : hash_table_[hash_key]) {
        std::vector<Value> out_values;
        
        // 缝合数据的顺序绝对不能变：先放左边，再放右边
        for (uint32_t i = 0; i < left_executor_->GetOutputSchema().GetColumnCount(); ++i) {
          out_values.push_back(left_tuple.GetValue(&left_executor_->GetOutputSchema(), i));
        }
        for (uint32_t i = 0; i < right_executor_->GetOutputSchema().GetColumnCount(); ++i) {
          out_values.push_back(matched_right_tuple.GetValue(&right_executor_->GetOutputSchema(), i));
        }
        
        result_tuples_.emplace_back(std::move(out_values));
      }
    }
  }
}

auto HashJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Probe hash table with right child tuples
  // - Phase 1 (in Init): Build hash table from left child on left_key_expr
  // - Phase 2 (in Next): For each right tuple, probe hash table using right_key_expr
  if (cursor_ < result_tuples_.size()) {
    *tuple = result_tuples_[cursor_];
    *rid = RID(INVALID_PAGE_ID, 0); 
    cursor_++;
    return true;
  }
  return false;

}

}  // namespace onebase
