#include "onebase/execution/executors/sort_executor.h"
#include <algorithm>
#include "onebase/common/exception.h"

namespace onebase {

SortExecutor::SortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                            std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void SortExecutor::Init() {
  // TODO(student): Materialize all tuples from child, then sort
  child_executor_->Init();
  sorted_tuples_.clear();
  cursor_=0;
  Tuple child_tuple;
  RID child_rid;
  while (child_executor_->Next(&child_tuple,&child_rid))
  // - Scan all child tuples into sorted_tuples_
  {
    sorted_tuples_.push_back(child_tuple);
  }
  // - Sort using order_by expressions and directions
  std::sort(sorted_tuples_.begin(),sorted_tuples_.end(),
            [this](const Tuple &a,const Tuple &b){
              for(const auto &[is_ascending,expr]:plan_->GetOrderBys()){
                Value val_a=expr->Evaluate(&a,&child_executor_->GetOutputSchema());
                Value val_b=expr->Evaluate(&b,&child_executor_->GetOutputSchema());
                if(val_a.CompareEquals(val_b).GetAsBoolean()){
                  continue;
                }
                if(is_ascending){
                  return val_a.CompareLessThan(val_b).GetAsBoolean();
                }else{
                  return val_a.CompareGreaterThan(val_b).GetAsBoolean();
                }
              }
              return false;
            });
  // - Reset cursor_ to 0
  
}

auto SortExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Return next sorted tuple
    if (cursor_ < sorted_tuples_.size()) {
      *tuple = sorted_tuples_[cursor_];
      *rid = RID(INVALID_PAGE_ID, 0); // 排序打乱了原本的物理存储顺序，返回无效的 RID
      cursor_++;
      return true;
    }
    return false;
}

}  // namespace onebase
