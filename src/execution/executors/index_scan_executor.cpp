#include "onebase/execution/executors/index_scan_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

void IndexScanExecutor::Init() {
  // TODO(student): Initialize index scan using the B+ tree index
  auto *catalog=GetExecutorContext()->GetCatalog();
  index_info_=catalog->GetIndex(plan_->GetIndexOid());
  table_info_=catalog->GetTable(plan_->GetTableOid());
  matching_rids_.clear();
  cursor_=0;

  auto lookup_expr=plan_->GetLookupKey();
  Value val = lookup_expr->Evaluate(nullptr, nullptr);
  int32_t raw_key=val.GetAsInteger();
  const std::vector<RID> *rids_ptr=index_info_->LookupInteger(raw_key);
  if(rids_ptr!=nullptr){
    matching_rids_=*rids_ptr;
  }
}

auto IndexScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Return next tuple from index scan
  if (cursor_ < matching_rids_.size()) {
    *rid = matching_rids_[cursor_];
    
    // O(1) 级别的精准打击！
    *tuple = table_info_->table_->GetTuple(*rid);
    
    cursor_++;
    return true;
  }
  
  return false;
}


}  // namespace onebase
