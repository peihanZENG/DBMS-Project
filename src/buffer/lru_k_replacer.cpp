#include "onebase/buffer/lru_k_replacer.h"
#include "onebase/common/exception.h"

namespace onebase {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k)
    : max_frames_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  // TODO(student): Implement LRU-K eviction policy
  std::lock_guard<std::mutex> lock(latch_);
  // - Find the frame with the largest backward k-distance
  frame_id_t victim_frame_id=-1;
  size_t victim_time=std::numeric_limits<size_t>::max();
  bool found_less_than_k=false;
  for (auto &it:this->entries_){
    auto f_id=it.first;
    auto &entry=it.second;

    if (!entry.is_evictable_){
      continue;
    }
    if (entry.history_.size()<k_){
      if (victim_time>entry.history_.front() || !found_less_than_k){
        victim_frame_id=f_id;
        victim_time=entry.history_.front();
        found_less_than_k=true;
      }
    }else{
      if (victim_time>entry.history_.front() && !found_less_than_k){
        victim_frame_id=f_id;
        victim_time=entry.history_.front();
      }
    }
  }
  // - Among frames with fewer than k accesses, evict the one with earliest first access
  // - Only consider evictable frames
  if (victim_frame_id!=-1){
    *frame_id=victim_frame_id;
    entries_.erase(victim_frame_id);
    curr_size_--;
    return true;
  }else{
    return false;
  }
  throw NotImplementedException("LRUKReplacer::Evict");
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  // TODO(student): Record a new access for frame_id at current timestamp
  // - If frame_id is new, create an entry
  if (static_cast<size_t>(frame_id) >= max_frames_){
    throw OneBaseException("invalid frameid",ExceptionType::OUT_OF_RANGE);
  }
  std::lock_guard<std::mutex> lock(latch_);
  // - Add current_timestamp_ to the frame's history
  auto &hist=entries_[frame_id].history_;
  if (hist.size()==k_){
    hist.pop_front();
    hist.push_back(current_timestamp_);
  }else{
    hist.push_back(current_timestamp_);
  }
  // - Increment current_timestamp_
  current_timestamp_++;
  return;
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  // TODO(student): Set whether a frame is evictable
  std::lock_guard<std::mutex> lock(latch_);
  auto it=entries_.find(frame_id);
  if (it == entries_.end()){
    // Fail to find 
    return;
  }
  auto fr_entr = &it->second;
  if (fr_entr->is_evictable_ != set_evictable){
    
    if (set_evictable==true){
      curr_size_++;
    }else{
      curr_size_--;
    }
    fr_entr->is_evictable_=set_evictable;
  }
  return;
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  // TODO(student): Remove a frame from the replacer
  // - The frame must be evictable; throw if not
  std::lock_guard<std::mutex> lock(latch_);
  auto it=entries_.find(frame_id);
  if (it == entries_.end()){
    // Fail to find 
    return;
  }
  auto fr_entr = &it->second;
  if (fr_entr->is_evictable_){
    entries_.erase(it);
    curr_size_--;
  }else{
    throw OneBaseException("Not Evictable While Removing.",ExceptionType::EXECUTION);
  }
  return;
  throw NotImplementedException("LRUKReplacer::Remove");
}



auto LRUKReplacer::Size() const -> size_t {
  // TODO(student): Return the number of evictable frames
  return this->curr_size_;
  throw NotImplementedException("LRUKReplacer::Size");
}

}  // namespace onebase
