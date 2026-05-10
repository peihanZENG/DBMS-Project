#include "onebase/execution/executors/seq_scan_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

SeqScanExecutor::SeqScanExecutor(ExecutorContext *exec_ctx, const SeqScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

void SeqScanExecutor::Init() {
  // TODO(student): Initialize the sequential scan
  // - Get the table from catalog using plan_->GetTableOid()
  ExecutorContext *exec_ctx_=GetExecutorContext();
  Catalog *catalog_=exec_ctx_->GetCatalog();
  table_info_=catalog_->GetTable(plan_->GetTableOid());
  // - Set up iterator to table_heap->Begin()
  iter_=table_info_->table_->Begin();
  end_=table_info_->table_->End();
}

auto SeqScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  while (iter_ != end_) {
    *rid = iter_.GetRID();
    // 1. 获取带有原始字节数据的 Tuple
    Tuple raw_tuple = table_info_->table_->GetTuple(*rid);
    
    ++iter_;

    // 2. 使用谓词进行条件判断（Evaluate 底层会处理 raw_tuple）
    const auto &predicate = plan_->GetPredicate();
    if (predicate != nullptr) {
      Value value = predicate->Evaluate(&raw_tuple, &table_info_->schema_);
      if (value.IsNull() || !value.GetAsBoolean()) {
        continue; // 不满足条件，检查下一行
      }
    }

    // 3. ✨ 关键修复：手动将原始字节反序列化为 Value 数组 ✨
    // 因为 raw_tuple 的 values_ 是空的，直接返回给上层会导致崩溃或 NULL。
    // 我们必须利用 schema 将每一列数据提取出来。
    std::vector<Value> parsed_values;
    uint32_t col_count = table_info_->schema_.GetColumnCount();
    for (uint32_t i = 0; i < col_count; ++i) {
      parsed_values.push_back(raw_tuple.GetValue(&table_info_->schema_, i));
    }

    // 4. 用解析好的 values 重新构造一个“健康”的 Tuple 并返回
    *tuple = Tuple(std::move(parsed_values));
    tuple->SetRID(*rid); // 别忘了把 RID 塞回去

    return true; // 成功返回一行数据
  }
  return false;
}

}  // namespace onebase
