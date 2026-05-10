#include "onebase/execution/executors/insert_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  // TODO(student): Initialize child executor
  child_executor_->Init();
  has_inserted_=false;
}

auto InsertExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Insert tuples from child into the table
  if (has_inserted_){
    return false;
  }

  ExecutorContext *exec_ctx=GetExecutorContext();
  auto *catalog=exec_ctx->GetCatalog();
  auto *table_info=catalog->GetTable(plan_->GetTableOid());
  auto indexes=catalog->GetTableIndexes(table_info->name_);

  int32_t insert_count=0;
  Tuple child_tuple;
  RID child_rid;
  while (child_executor_->Next(&child_tuple,&child_rid))
  // - Get tuples from child, insert into table_heap
  {
    auto new_rid_opt=table_info->table_->InsertTuple(child_tuple);
    if(new_rid_opt.has_value()){
      RID new_rid=new_rid_opt.value();
      insert_count++;
  // - Update any indexes
      for (auto *index_info:indexes){
        if (index_info->SupportsPointLookup()){
          int32_t key=child_tuple.GetValue(&table_info->schema_,index_info->GetLookupAttr()).GetAsInteger();
          index_info->InsertEntry(key,new_rid);
        }
      }
    }
  }
  // - Return count of inserted rows as a single integer tuple
  std::vector<Value> values;
  values.emplace_back(TypeId::INTEGER,insert_count);
  *tuple=Tuple(std::move(values));
  has_inserted_=true;
  return true;
}

}  // namespace onebase
