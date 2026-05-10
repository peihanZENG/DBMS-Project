#include "onebase/storage/page/b_plus_tree_leaf_page.h"
#include <functional>
#include "onebase/common/exception.h"

namespace onebase {

template class BPlusTreeLeafPage<int, RID, std::less<int>>;

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
  SetPageType(IndexPageType::LEAF_PAGE);
  SetMaxSize(max_size);
  SetSize(0);
  SetParentPageId(INVALID_PAGE_ID);
  next_page_id_ = INVALID_PAGE_ID;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  return array_[index].first;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  return array_[index].second;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  // TODO(student): Binary search for the index of key
  int low=0;int high=GetSize()-1;
  while (low<=high){
    int mid=(low+high)/2;
    if (comparator(KeyAt(mid),key)){
      low=mid+1;
    }else{
      high=mid-1;
    }
  }
  return low;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::Lookup(const KeyType &key, ValueType *value,
                                         const KeyComparator &comparator) const -> bool {
  // TODO(student): Look up a key and return its associated value
  int index=KeyIndex(key,comparator);
  if (index<GetSize() && !comparator(key,KeyAt(index)) && !comparator(KeyAt(index),key)){
    *value=ValueAt(index);
    return true;
  }
  return false;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::Insert(const KeyType &key, const ValueType &value,
                                         const KeyComparator &comparator) -> int {
  // TODO(student): Insert a key-value pair in sorted order
  int index=KeyIndex(key,comparator);
  if (index<GetSize() && !comparator(key,KeyAt(index)) && !comparator(KeyAt(index),key)){
    //如果发生重复
    return GetSize();
  }
  for (int i=GetSize();i>index;i--){
    array_[i]=array_[i-1];
  }
  array_[index]={key,value};
  IncreaseSize(1);
  return GetSize();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::RemoveAndDeleteRecord(const KeyType &key,
                                                        const KeyComparator &comparator) -> int {
  // TODO(student): Remove a key-value pair
  int index=KeyIndex(key,comparator);
  if (index>=GetSize() || comparator(key,KeyAt(index)) || comparator(KeyAt(index),key)){
    return GetSize();
  }
  for (int i=index;i<GetSize()-1;i++){
    array_[i]=array_[i+1];
  }
  IncreaseSize(-1);
  return GetSize();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveHalfTo(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move second half of entries to recipient during split
  // ? 压根没有recipient的page_id,只能在外面加
  int split_idx=GetSize()/2;
  for (int i=split_idx;i<GetSize();i++){
    recipient->array_[i-split_idx]=array_[i];
  }
  recipient->SetSize(GetSize()-split_idx);
  SetSize(split_idx);
  // recipient->SetNextPageId(next_page_id_);
  // recipient->SetParentPageId(GetParentPageId());
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveAllTo(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move all entries to recipient during merge
  // recipient is left leaf
  int start_idx=recipient->GetSize();
  for (int i=0;i<GetSize();i++){
    recipient->array_[start_idx+i]=array_[i];
  }
  recipient->IncreaseSize(GetSize());
  recipient->SetNextPageId(GetNextPageId());
  SetSize(0);
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move first entry to end of recipient
  recipient->array_[recipient->GetSize()]=array_[0];
  for (int i=0;i<GetSize()-1;i++){
    array_[i]=array_[i+1];
  }
  IncreaseSize(-1);
  recipient->IncreaseSize(1);
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move last entry to front of recipient
  for (int i=recipient->GetSize();i>0;i--){
    recipient->array_[i]=recipient->array_[i-1];
  }
  recipient->array_[0]=array_[GetSize()-1];
  IncreaseSize(-1);
  recipient->IncreaseSize(1);
  return;
}

}  // namespace onebase
