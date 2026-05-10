#include "onebase/execution/executors/nested_loop_join_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx,
                                                const NestedLoopJoinPlanNode *plan,
                                                std::unique_ptr<AbstractExecutor> left_executor,
                                                std::unique_ptr<AbstractExecutor> right_executor)
    : AbstractExecutor(exec_ctx), plan_(plan),
      left_executor_(std::move(left_executor)), right_executor_(std::move(right_executor)) {}

void NestedLoopJoinExecutor::Init() {
  // TODO(student): Initialize both child executors
  left_executor_->Init();
  result_tuples_.clear();
  cursor_ = 0;

  Tuple left_tuple;
  RID left_rid;

  while(left_executor_->Next(&left_tuple,&left_rid)){
    right_executor_->Init();
    Tuple right_tuple;
    RID right_rid;
    while (right_executor_->Next(&right_tuple,&right_rid))
    {
      bool is_match=true;
      const auto &predicate=plan_->GetPredicate();
      if (predicate!=nullptr){
        Value value=predicate->EvaluateJoin(&left_tuple,&left_executor_->GetOutputSchema(),
                                          &right_tuple,&right_executor_->GetOutputSchema());
        is_match=!value.IsNull() && value.GetAsBoolean();
      }
      if(is_match){
        std::vector<Value> out_values;
        for (uint32_t i=0;i<left_executor_->GetOutputSchema().GetColumnCount();++i){
          out_values.push_back(left_tuple.GetValue(&left_executor_->GetOutputSchema(),i));
        }
        for (uint32_t i = 0; i < right_executor_->GetOutputSchema().GetColumnCount(); ++i) {
          out_values.push_back(right_tuple.GetValue(&right_executor_->GetOutputSchema(), i));
        }
        result_tuples_.emplace_back(std::move(out_values));
      }
    }
    
  }
}

auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Perform nested loop join
  // - For each left tuple, scan all right tuples
  // - Evaluate predicate on (left, right) pairs
  // - Output matching combined tuples
  if (cursor_ < result_tuples_.size()) {
    *tuple = result_tuples_[cursor_];
    *rid = RID(INVALID_PAGE_ID, 0); // Join 产生的新行没有实质的物理位置
    cursor_++;
    return true;
  }
  return false;
}

}  // namespace onebase
