#include "onebase/buffer/buffer_pool_manager.h"
#include "onebase/common/exception.h"
#include "onebase/common/logger.h"

namespace onebase {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<frame_id_t>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * {
  std::lock_guard<std::mutex> lock(latch_);
  // TODO(student): Allocate a new page in the buffer pool
  // 1. Pick a victim frame from free list or replacer
  frame_id_t f_id=-1;
  if (!free_list_.empty()){
    f_id=free_list_.front();
    free_list_.pop_front();
  }else{
    if (!replacer_->Evict(&f_id)){
      return nullptr;
    }else{
      Page *victim_page=& pages_[f_id];
  // 2. If victim is dirty, write it back to disk
      if (victim_page->IsDirty()){
        disk_manager_->WritePage(victim_page->GetPageId(),victim_page->GetData());
      }
      page_table_.erase(victim_page->GetPageId());
    }
  }
  // 3. Allocate a new page_id via disk_manager_
  *page_id=disk_manager_->AllocatePage();
  Page *new_page = &pages_[f_id];
  new_page->ResetMemory();
  new_page->page_id_ = *page_id;
  new_page->pin_count_ = 1;
  new_page->is_dirty_ = false;
  // 4. Update page_table_ and page metadata
  page_table_[*page_id]=f_id;
  replacer_->RecordAccess(f_id);
  replacer_->SetEvictable(f_id, false);
  return new_page;
}

auto BufferPoolManager::FetchPage(page_id_t page_id) -> Page * {
  // TODO(student): Fetch a page from the buffer pool
  std::lock_guard<std::mutex> lock(latch_);

  // 1. Search page_table_ for existing mapping
  frame_id_t f_id=-1;
  Page *page=nullptr;
  auto it=page_table_.find(page_id);
  if (it!=page_table_.end()){
      f_id=it->second;
      page=&pages_[f_id];
      page->pin_count_++;

      replacer_->RecordAccess(f_id);
      replacer_->SetEvictable(f_id,false);
      return page;
  }else{
  // 2. If not found, pick a victim frame
    if (!free_list_.empty()){
      f_id=free_list_.front();
      free_list_.pop_front();
    }else{
      // 尝试驱逐得到frame
      bool success=replacer_->Evict(&f_id);
      if (!success){
        // throw OneBaseException("no free frames",ExceptionType::BUFFER_FULL);
        return nullptr;
      }
      Page *victim_page=& pages_[f_id];
      if (victim_page->IsDirty()){
        disk_manager_->WritePage(victim_page->GetPageId(),victim_page->GetData());
      }
      page_table_.erase(victim_page->GetPageId());
    }
  // 3. Read page from disk into the frame
  page=&pages_[f_id];
  disk_manager_->ReadPage(page_id,page->data_);

  page->is_dirty_=false;
  page->pin_count_=1;
  page->page_id_=page_id;

  page_table_[page_id]=f_id;
  replacer_->RecordAccess(f_id);
  replacer_->SetEvictable(f_id,false);
  return page;
  }
}

auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) -> bool {
  // TODO(student): Unpin a page, decrementing pin count
  std::lock_guard<std::mutex> lock(latch_);
  auto it=page_table_.find(page_id);
  if (it==page_table_.end()){
    return false;
  }
  frame_id_t f_id=it->second;
  Page *page=&pages_[f_id];
  if (page->pin_count_<=0){
    return false;
  }
  page->pin_count_--;
  // - If pin_count reaches 0, set evictable in replacer
  if (is_dirty){
    page->is_dirty_=true;
  }
  if (page->pin_count_==0){
    replacer_->SetEvictable(f_id,true);
  }
  return true;
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  // TODO(student): Delete a page from the buffer pool
  //用途:数据彻底作废时调用
  std::lock_guard<std::mutex> lock(latch_);
  auto it=page_table_.find(page_id);
  if (it==page_table_.end()){
    return true;
  }
  frame_id_t f_id=it->second;
  Page *page=&pages_[f_id];
  // - Page must have pin_count == 0
  if (page->pin_count_!=0){
    return false;
  }
  // - Remove from page_table_, reset memory, add frame to free_list_
  page_table_.erase(it);
  replacer_->Remove(f_id);
  free_list_.push_back(f_id);

  page->ResetMemory();
  page->page_id_=INVALID_PAGE_ID;
  page->pin_count_=0;
  page->is_dirty_=false;

  disk_manager_->DeallocatePage(page_id);
  return true;
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  // TODO(student): Force flush a page to disk regardless of dirty flag
  std::lock_guard<std::mutex> lock(latch_);
  auto it=page_table_.find(page_id);
  if (it==page_table_.end()){
    return false;
  }
  auto f_id=it->second;
  Page *page=&pages_[f_id];
  disk_manager_->WritePage(page_id,page->data_);
  page->is_dirty_=false;
  return true;
}

void BufferPoolManager::FlushAllPages() {
  // TODO(student): Flush all pages in the buffer pool to disk
  std::lock_guard<std::mutex> lock(latch_);
  for (auto const &[p_id,f_id]:page_table_){
    Page *page=&pages_[f_id];
    disk_manager_->WritePage(p_id,page->data_);
    page->is_dirty_=false;
  }
  return;
}

}  // namespace onebase
