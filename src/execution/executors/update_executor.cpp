#include "onebase/execution/executors/update_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

UpdateExecutor::UpdateExecutor(ExecutorContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void UpdateExecutor::Init() {
  // TODO(student): Initialize child executor
  child_executor_->Init();
  has_updated_=false;
}

auto UpdateExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Update tuples using update expressions
  if (has_updated_){
    return false;
  }
  auto *exec_ctx=GetExecutorContext();
  auto *catalog = exec_ctx->GetCatalog();
  auto *table_info = catalog->GetTable(plan_->GetTableOid());
  auto indexes = catalog->GetTableIndexes(table_info->name_);

  int32_t update_count = 0;
  Tuple child_tuple;
  RID child_rid;
  while (child_executor_->Next(&child_tuple,&child_rid))
  // - Get tuples from child, evaluate update expressions, update table_heap
  {
    std::vector<Value> new_values;
    const auto &exprs=plan_->GetUpdateExpressions();
    for (const auto &expr:exprs){
      new_values.push_back(expr->Evaluate(&child_tuple,&child_executor_->GetOutputSchema()));
    }
    Tuple new_tuple(std::move(new_values));
    table_info->table_->UpdateTuple(child_rid,new_tuple);
    update_count++;
    
    for (auto*index_info:indexes){
      if(index_info->SupportsPointLookup()){
        int32_t old_key = child_tuple.GetValue(&table_info->schema_, index_info->GetLookupAttr()).GetAsInteger();
        int32_t new_key = new_tuple.GetValue(&table_info->schema_, index_info->GetLookupAttr()).GetAsInteger();

        index_info->RemoveEntry(old_key,child_rid);
        index_info->InsertEntry(new_key,child_rid);
      }
    }
  }
  // - Return count of updated rows
  std::vector<Value> values{{TypeId::INTEGER, update_count}};
  *tuple = Tuple(std::move(values));
  has_updated_ = true;
  return true;
}

}  // namespace onebase
