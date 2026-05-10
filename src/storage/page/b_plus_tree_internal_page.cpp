#include "onebase/storage/page/b_plus_tree_internal_page.h"
#include <functional>
#include "onebase/common/exception.h"

namespace onebase {

template class BPlusTreeInternalPage<int, page_id_t, std::less<int>>;

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(int max_size) {
  SetPageType(IndexPageType::INTERNAL_PAGE);
  SetMaxSize(max_size);
  SetSize(0);
  SetParentPageId(INVALID_PAGE_ID);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  return array_[index].first;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
  array_[index].first = key;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  return array_[index].second;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetValueAt(int index, const ValueType &value) {
  array_[index].second = value;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const -> int {
  // TODO(student): Find the index of the given value in the internal page
  // TODO(student): Find the index of the given value in the internal page
  for (int i=0;i<GetSize();i++){
    if (array_[i].second == value){
      return i;
    }
  }
  return -1;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType {
  // TODO(student): Find the child page that should contain the given key
  int low=1;int high=GetSize()-1;
  int index=0;
  while (low<=high){
    int mid=(low+high)/2;
    if (comparator(key,KeyAt(mid))){
      high=mid-1;
    }else{
      index=mid;
      low=mid+1;
    }
  }
  return ValueAt(index);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::PopulateNewRoot(const ValueType &old_value, const KeyType &key,
                                                      const ValueType &new_value) {
  // TODO(student): Create a new root with one key and two children
  SetValueAt(0,old_value);
  SetKeyAt(1,key);
  SetValueAt(1,new_value);
  SetSize(2);
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertNodeAfter(const ValueType &old_value, const KeyType &key,
                                                      const ValueType &new_value) -> int {
  // TODO(student): Insert a new key-value pair after old_value
  int idx=ValueIndex(old_value);
  for (int i=GetSize();i>idx+1;i--){
    SetValueAt(i,ValueAt(i-1));
    SetKeyAt(i,KeyAt(i-1));
  }
  SetValueAt(idx+1,new_value);
  SetKeyAt(idx+1,key);
  IncreaseSize(1);
  return GetSize();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Remove(int index) {
  // TODO(student): Remove the key-value pair at the given index
  for (int i=index;i<GetSize()-1;i++){
    SetValueAt(i,ValueAt(i+1));
    SetKeyAt(i,KeyAt(i+1));
  }
  IncreaseSize(-1);
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::RemoveAndReturnOnlyChild() -> ValueType {
  // TODO(student): Remove all entries and return the only remaining child
  if (GetSize()!=1){
    return INVALID_PAGE_ID;
  }
  ValueType child=ValueAt(0);
  SetSize(0);
  return child;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAllTo(BPlusTreeInternalPage *recipient, const KeyType &middle_key) {
  // TODO(student): Move all entries to recipient during merge
  int d_idx=recipient->GetSize();
  recipient->SetKeyAt(d_idx,middle_key);
  recipient->SetValueAt(d_idx,ValueAt(0));
  for (int i=1;i<GetSize();i++){
    recipient->SetValueAt(d_idx+i,ValueAt(i));
    recipient->SetKeyAt(d_idx+i,KeyAt(i));
  }
  recipient->IncreaseSize(GetSize());
  SetSize(0);
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveHalfTo(BPlusTreeInternalPage *recipient, const KeyType &middle_key) {
  // TODO(student): Move the second half of entries to recipient during split
  int split_idx=GetSize()/2;
  int move=GetSize()-split_idx;
  // middle_key=ValueAt(split_idx);
  for (int i=0;i<move;i++){
    recipient->SetValueAt(i,ValueAt(split_idx+i));
    recipient->SetKeyAt(i,KeyAt(split_idx+i));
  }
  recipient->IncreaseSize(move);
  IncreaseSize(-move);
  return;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeInternalPage *recipient, const KeyType &middle_key) {
  // TODO(student): Move first entry to end of recipient (redistribute)
  recipient->SetValueAt(recipient->GetSize(),ValueAt(0));
  recipient->SetKeyAt(recipient->GetSize(),middle_key);
  SetValueAt(0,ValueAt(1));
  for (int i=1;i<GetSize()-1;i++){
    SetValueAt(i,ValueAt(i+1));
    SetKeyAt(i,KeyAt(i+1)); 
  }
  IncreaseSize(-1);
  recipient->IncreaseSize(1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeInternalPage *recipient, const KeyType &middle_key) {
  // TODO(student): Move last entry to front of recipient (redistribute)
  for (int i=recipient->GetSize();i>0;i--){
    recipient->SetValueAt(i,recipient->ValueAt(i-1));
    recipient->SetKeyAt(i,recipient->KeyAt(i-1));
  }
  recipient->SetValueAt(0,ValueAt(GetSize()-1));
  recipient->SetKeyAt(1,middle_key);
  IncreaseSize(-1);
  recipient->IncreaseSize(1);
  return;
}

}  // namespace onebase
