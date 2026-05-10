#include "onebase/execution/executors/delete_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void DeleteExecutor::Init() {
  // TODO(student): Initialize child executor
  child_executor_->Init();
  has_deleted_=false;
}

auto DeleteExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Delete tuples identified by child executor
  if (has_deleted_){
    return false;
  }
  auto *exec_ctx=GetExecutorContext();
  auto *catalog = exec_ctx->GetCatalog();
  auto *table_info = catalog->GetTable(plan_->GetTableOid());
  auto indexes = catalog->GetTableIndexes(table_info->name_);

  int32_t delete_count = 0;
  Tuple child_tuple;
  RID child_rid;
  while (child_executor_->Next(&child_tuple,&child_rid)){
    // - Get tuples from child, delete from table_heap
    table_info->table_->DeleteTuple(child_rid);
    delete_count++;

    // - Update any indexes
    for (auto *index_info:indexes){
      if (index_info->SupportsPointLookup()){
        int32_t key=child_tuple.GetValue(&table_info->schema_,index_info->GetLookupAttr()).GetAsInteger();
        index_info->RemoveEntry(key,child_rid);
      }
    }
  }
  // - Return count of deleted rows
  std::vector<Value> values{{TypeId::INTEGER, delete_count}};
  *tuple = Tuple(std::move(values));
  has_deleted_ = true;
  return true;
}

}  // namespace onebase
