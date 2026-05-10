#include "onebase/storage/index/b_plus_tree.h"
#include "onebase/storage/index/b_plus_tree_iterator.h"
#include <functional>
#include "onebase/common/exception.h"


namespace onebase {

template class BPlusTree<int, RID, std::less<int>>;

template <typename KeyType, typename ValueType, typename KeyComparator>
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *bpm, const KeyComparator &comparator,
                           int leaf_max_size, int internal_max_size)
    : Index(std::move(name)), bpm_(bpm), comparator_(comparator),
      leaf_max_size_(leaf_max_size), internal_max_size_(internal_max_size) {
  if (leaf_max_size_ == 0) {
    leaf_max_size_ = static_cast<int>(
        (ONEBASE_PAGE_SIZE - sizeof(BPlusTreePage) - sizeof(page_id_t)) /
        (sizeof(KeyType) + sizeof(ValueType)));
  }
  if (internal_max_size_ == 0) {
    internal_max_size_ = static_cast<int>(
        (ONEBASE_PAGE_SIZE - sizeof(BPlusTreePage)) /
        (sizeof(KeyType) + sizeof(page_id_t)));
  }
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  return root_page_id_ == INVALID_PAGE_ID;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  // TODO(student): Insert a key-value pair into the B+ tree
  // 1. If tree is empty, create a new leaf root
  if (IsEmpty()) {
    page_id_t new_page_id;
    Page *page = bpm_->NewPage(&new_page_id);
    if (page == nullptr) {
      return false;
    }
    root_page_id_ = new_page_id;
    BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
    leaf->Init(leaf_max_size_);
    leaf->Insert(key, value, comparator_);
    bpm_->UnpinPage(page->GetPageId(), true);
    return true;
  }
  
  // 2. Find the leaf page for key
  page_id_t page_id = root_page_id_;
  Page *page = bpm_->FetchPage(page_id);
  auto *tree_page=reinterpret_cast<BPlusTreePage *>(page->GetData());
  while (!tree_page->IsLeafPage()) {
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(page->GetData());
    page_id_t next_page_id =internal->Lookup(key, comparator_);
    bpm_->UnpinPage(page_id, false);
    page_id=next_page_id;
    page=bpm_->FetchPage(page_id);
    tree_page=reinterpret_cast<BPlusTreePage *>(page->GetData());
  }

  // 3. Insert into leaf; if overflow, split and propagate up
  auto leaf=reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
  // insertion
  int old_size = leaf->GetSize();
  int new_size = leaf->Insert(key, value, comparator_);
  if (old_size==new_size){
    // fail to insert
    bpm_->UnpinPage(page->GetPageId(), false);
    return false;
  }
  if (leaf->GetSize()<=leaf->GetMaxSize()) {
    //leaf not overflow
    bpm_->UnpinPage(page->GetPageId(), true);
    return true;
  }

  //overflow
  //create new page
  page_id_t right_page_id;
  Page* new_page=bpm_->NewPage(&right_page_id);
  BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *new_leaf=reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(new_page->GetData());
  new_leaf->Init(leaf_max_size_);
  new_leaf->SetParentPageId(leaf->GetParentPageId());
  new_leaf->SetNextPageId(leaf->GetNextPageId());
  leaf->SetNextPageId(right_page_id);
  leaf->MoveHalfTo(new_leaf);
  KeyType middle_key=new_leaf->KeyAt(0);
  BPlusTreePage *left_page=leaf;
  BPlusTreePage *right_page=new_leaf;
  page_id_t left_page_id=page_id;

  while (true){
    if (left_page->IsRootPage()){
      page_id_t parent_page_id;
      Page* parent_page=bpm_->NewPage(&parent_page_id);
      BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> * parent_node=reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(parent_page->GetData());
      parent_node->Init(internal_max_size_);
      parent_node->PopulateNewRoot(left_page_id, middle_key, right_page_id);
      root_page_id_=parent_page_id;
      left_page->SetParentPageId(parent_page_id);
      right_page->SetParentPageId(parent_page_id);

      bpm_->UnpinPage(parent_page_id, true);
      bpm_->UnpinPage(left_page_id, true);
      bpm_->UnpinPage(right_page_id, true);
      return true;
    }
    
    //there is a parent,start with two nodes
    page_id_t parent_page_id =left_page->GetParentPageId();
    right_page->SetParentPageId(parent_page_id);
    Page *parent_page=bpm_->FetchPage(parent_page_id);
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *parent_node=reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(parent_page->GetData());
    parent_node->InsertNodeAfter(left_page_id, middle_key,right_page_id);
    bpm_->UnpinPage(left_page_id, true);
    bpm_->UnpinPage(right_page_id, true);
    if (parent_node->GetSize()<=parent_node->GetMaxSize()){
      // no more overflow
      bpm_->UnpinPage(parent_page_id, true);
      return true;
    }
    //parent overflow
    left_page=parent_node;
    left_page_id=parent_page_id;
    new_page=bpm_->NewPage(&right_page_id);
    right_page=reinterpret_cast<BPlusTreePage *>(new_page->GetData());
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *new_node=reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(new_page->GetData());
    new_node->Init(internal_max_size_);
    new_node->SetParentPageId(parent_node->GetParentPageId());
    KeyType new_middle_key = parent_node->KeyAt(parent_node->GetSize() / 2);
    parent_node->MoveHalfTo(new_node, new_middle_key);
    middle_key=new_middle_key;
    //update children of the new internal page to point to it as parent
    for (int i = 0; i < new_node->GetSize(); i++) {
      page_id_t child_id = new_node->ValueAt(i);
      Page *child_page = bpm_->FetchPage(child_id);
      auto *child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(right_page_id);
      bpm_->UnpinPage(child_id, true);
    }
  }
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  // TODO(student): Remove a key from the B+ tree
  // 1. Find the leaf page containing key
  if (root_page_id_==INVALID_PAGE_ID){return;}
  page_id_t right_page_id=root_page_id_;
  page_id_t parent_page_id=INVALID_PAGE_ID;
  Page * right_page=bpm_->FetchPage(right_page_id);
  BPlusTreePage *tree_page=reinterpret_cast<BPlusTreePage *>(right_page->GetData());
  while (! tree_page->IsLeafPage()) {
    auto *internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(right_page->GetData());
    page_id_t next_page_id = internal->Lookup(key, comparator_);
    bpm_->UnpinPage(right_page_id, false);

    right_page_id=next_page_id;
    right_page=bpm_->FetchPage(right_page_id);
    tree_page=reinterpret_cast<BPlusTreePage *>(right_page->GetData());
  }
  BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *parent_internal=nullptr;
  BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *right_leaf_page=reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(right_page->GetData());
  parent_page_id = right_leaf_page->GetParentPageId();
  Page *parent_page = nullptr;
  if (parent_page_id != INVALID_PAGE_ID) {
    parent_page = bpm_->FetchPage(parent_page_id);
    parent_internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(parent_page->GetData());
  }

  // 2. Remove from leaf; if underflow, merge or redistribute
  //remove from the leaf
  int old_size=right_leaf_page->GetSize();
  int new_size=right_leaf_page->RemoveAndDeleteRecord(key, comparator_);
  if (new_size==old_size){
    bpm_->UnpinPage(right_page_id, false);
    if (parent_page_id != INVALID_PAGE_ID) {
        bpm_->UnpinPage(parent_page_id, false);
    }
    //key not found, return
    return;
  }
  if(new_size>=right_leaf_page->GetMinSize()){
    //not underflow
    bpm_->UnpinPage(right_page_id, true);
    if (parent_page_id != INVALID_PAGE_ID) {
        bpm_->UnpinPage(parent_page_id, true);
    }
    return;
  }

  //leaf underflow
  if (right_leaf_page->IsRootPage()){
    if (right_leaf_page->GetSize()==0){
      root_page_id_=INVALID_PAGE_ID;
      bpm_->UnpinPage(right_page_id, true);
      bpm_->DeletePage(right_page_id);
    }else{
      bpm_->UnpinPage(right_page_id, true);
    }
    
    return;
  }
  // assume right overflow initially
  int right_page_index=parent_internal->ValueIndex(right_page_id);
  int left_page_index=right_page_index-1;
  page_id_t left_page_id;
  BPlusTreeLeafPage<KeyType, ValueType,KeyComparator> *left_leaf_page;
  bool right_underflow=true;
  if (right_page_index==0){
    //no more left indexes
    right_underflow=false;
    left_page_index=right_page_index;
    right_page_index+=1;

    left_page_id=right_page_id;
    right_page_id=parent_internal->ValueAt(right_page_index);
    
    Page *right_page=bpm_->FetchPage(right_page_id);

    left_leaf_page=right_leaf_page;
    right_leaf_page=reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType,KeyComparator> *>(right_page->GetData());

  }else{
    left_page_index=right_page_index-1;
    left_page_id=parent_internal->ValueAt(left_page_index);
    
    Page *left_page=bpm_->FetchPage(left_page_id);
    left_leaf_page=reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType,KeyComparator> *>(left_page->GetData());
  }
  
  if (left_leaf_page->GetSize()>left_leaf_page->GetMinSize() ||right_leaf_page->GetSize()>right_leaf_page->GetMinSize()){
    //borrow
    if (right_underflow){
      left_leaf_page->MoveLastToFrontOf(right_leaf_page);
      parent_internal->SetKeyAt(right_page_index, right_leaf_page->KeyAt(0));
      bpm_->UnpinPage(left_page_id, true);
      bpm_->UnpinPage(right_page_id, true);
      bpm_->UnpinPage(parent_page_id, true);
      return;
    }
    else{
      right_leaf_page->MoveFirstToEndOf(left_leaf_page);
      parent_internal->SetKeyAt(right_page_index, right_leaf_page->KeyAt(0));
      bpm_->UnpinPage(left_page_id, true);
      bpm_->UnpinPage(right_page_id, true);
      bpm_->UnpinPage(parent_page_id, true);
      return;
    }
  }else{
    //merge
    right_leaf_page->MoveAllTo(left_leaf_page);
    parent_internal->Remove(right_page_index);
    bpm_->UnpinPage(left_page_id, true);
    bpm_->UnpinPage(right_page_id, true);
    bpm_->DeletePage(right_page_id);
  }
  if (parent_internal->GetSize()>=parent_internal->GetMinSize()){
    bpm_->UnpinPage(parent_page_id, true);
    return;
  }

  //here recursion steps in

  // only the parent internal & parent page id variables are kept over loop
  //entering the loop already means parent underflow
  while (true) {
    //parent is root
    if (parent_internal->IsRootPage()){
      if (parent_internal->GetSize()==1){
        root_page_id_= parent_internal->RemoveAndReturnOnlyChild();
        Page *new_root_page = bpm_->FetchPage(root_page_id_);
        auto *new_root_node = reinterpret_cast<BPlusTreePage *>(new_root_page->GetData());
        new_root_node->SetParentPageId(INVALID_PAGE_ID);
        bpm_->UnpinPage(root_page_id_, true);
        
        bpm_->UnpinPage(parent_page_id, true); // 删之前先 Unpin
        bpm_->DeletePage(parent_page_id);
    }else{
      bpm_->UnpinPage(parent_page_id, true);
    }
      return;
    }
    //parent is not root, get prepared for recursion
    //交接
    right_page_id=parent_page_id;
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *right_child_internal=parent_internal;  //assume right, changed later.
    parent_page_id=parent_internal->GetParentPageId();
    Page *parent_page=bpm_->FetchPage(parent_page_id);
    parent_internal=reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(parent_page->GetData());
    right_page_index=parent_internal->ValueIndex(right_page_id);
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *left_child_internal;
    bool right_underflow=true;

    if (right_page_index==0){
      right_underflow=false;
      //no left page any more, borrow/merge with left
      left_page_id=right_page_id;
      left_child_internal=right_child_internal;
      left_page_index=right_page_index;
      right_page_index+=1;
      right_page_id=parent_internal->ValueAt(right_page_index);
      Page *right_page=bpm_->FetchPage(right_page_id);
      right_child_internal=reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(right_page->GetData());
    }else {
      //normally
      left_page_index=right_page_index-1;
      left_page_id=parent_internal->ValueAt(left_page_index);
      Page *left_page=bpm_->FetchPage(left_page_id);
      left_child_internal=reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(left_page->GetData());
    }
    KeyType middle_key=parent_internal->KeyAt(right_page_index);
    //deal with borrow
    if (left_child_internal->GetSize()>left_child_internal->GetMinSize() || right_child_internal->GetSize()>right_child_internal->GetMinSize()){
      if (right_underflow){
        //borrow from left
        KeyType key_to_push_up = left_child_internal->KeyAt(left_child_internal->GetSize() - 1);
        left_child_internal->MoveLastToFrontOf(right_child_internal, middle_key);
        parent_internal->SetKeyAt(right_page_index, key_to_push_up);
      }else{
        //borrow from right
        KeyType key_to_push_up = right_child_internal->KeyAt(1);
        right_child_internal->MoveFirstToEndOf(left_child_internal, middle_key);
        parent_internal->SetKeyAt(right_page_index, key_to_push_up);
      }
      //update the moved child's parent pointer
      page_id_t moved_child_id = right_underflow ? right_child_internal->ValueAt(0):left_child_internal->ValueAt(left_child_internal->GetSize()-1);
      Page *moved_child_page = bpm_->FetchPage(moved_child_id);
      auto *moved_child = reinterpret_cast<BPlusTreePage *>(moved_child_page->GetData());
      moved_child->SetParentPageId(right_underflow ? right_page_id : left_page_id);
      bpm_->UnpinPage(moved_child_id, true);

      bpm_->UnpinPage(parent_page_id, true);
      bpm_->UnpinPage(left_page_id, true);
      bpm_->UnpinPage(right_page_id, true);
      return;
    }
    //deal with merge (right to left)
    right_child_internal->MoveAllTo(left_child_internal, middle_key);
    for (int i = 0; i < left_child_internal->GetSize(); i++) {
      page_id_t child_id = left_child_internal->ValueAt(i);
      Page *child_page = bpm_->FetchPage(child_id);
      auto *child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(left_page_id);
      bpm_->UnpinPage(child_id, true);
  }
    parent_internal->Remove(right_page_index);
    bpm_->UnpinPage(right_page_id, true);
    bpm_->DeletePage(right_page_id);
    bpm_->UnpinPage(left_page_id, true);
    //parent doesn't underflow anymore
    if (parent_internal->GetSize()>=parent_internal->GetMinSize()){
      bpm_->UnpinPage(parent_page_id, true);
      return;
    }
    // parent underflow, go to next loop
  }
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  // TODO(student): Search for key and add matching values to result
    if (IsEmpty()) {
    return false;
  }

  page_id_t curr_page_id = root_page_id_;
  auto *curr_page = bpm_->FetchPage(curr_page_id);
  auto *tree_page = reinterpret_cast<BPlusTreePage *>(curr_page->GetData());

  while (!tree_page->IsLeafPage()) {
    auto *internal_page = reinterpret_cast<InternalPage *>(tree_page);
    page_id_t next_page_id = internal_page->Lookup(key, comparator_);
    
    bpm_->UnpinPage(curr_page_id, false);
    
    curr_page_id = next_page_id;
    curr_page = bpm_->FetchPage(curr_page_id);
    tree_page = reinterpret_cast<BPlusTreePage *>(curr_page->GetData());
  }

  // 此时 tree_page 已经是叶子节点
  auto *leaf_page = reinterpret_cast<LeafPage *>(tree_page);
  ValueType val;
  bool found = leaf_page->Lookup(key, &val, comparator_);
  
  if (found) {
    result->push_back(val);
  }
  bpm_->UnpinPage(curr_page_id, false);
  return found;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto BPLUSTREE_TYPE::Begin() -> Iterator {
  // TODO(student): Return an iterator pointing to the first key
  if (IsEmpty()) {
    return Iterator(INVALID_PAGE_ID, 0, bpm_);
  }

  page_id_t curr_page_id = root_page_id_;
  auto *curr_page = bpm_->FetchPage(curr_page_id);
  auto *tree_page = reinterpret_cast<BPlusTreePage *>(curr_page->GetData());

  while (!tree_page->IsLeafPage()) {
    auto *internal_page = reinterpret_cast<InternalPage *>(tree_page);
    page_id_t next_page_id = internal_page->ValueAt(0); // 永远拿最左侧的指针
    
    bpm_->UnpinPage(curr_page_id, false);
    
    curr_page_id = next_page_id;
    curr_page = bpm_->FetchPage(curr_page_id);
    tree_page = reinterpret_cast<BPlusTreePage *>(curr_page->GetData());
  }

  bpm_->UnpinPage(curr_page_id, false);
  return Iterator(curr_page_id, 0, bpm_);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> Iterator {
  // TODO(student): Return an iterator pointing to the given key
    if (IsEmpty()) {
    return Iterator(INVALID_PAGE_ID, 0, bpm_);
  }

  page_id_t curr_page_id = root_page_id_;
  auto *curr_page = bpm_->FetchPage(curr_page_id);
  auto *tree_page = reinterpret_cast<BPlusTreePage *>(curr_page->GetData());

  while (!tree_page->IsLeafPage()) {
    auto *internal_page = reinterpret_cast<InternalPage *>(tree_page);
    page_id_t next_page_id = internal_page->Lookup(key, comparator_);
    bpm_->UnpinPage(curr_page_id, false);
    
    curr_page_id = next_page_id;
    curr_page = bpm_->FetchPage(curr_page_id);
    tree_page = reinterpret_cast<BPlusTreePage *>(curr_page->GetData());
  }
  LeafPage *leaf_page = reinterpret_cast<LeafPage *>(tree_page);

  int index=leaf_page->KeyIndex(key,comparator_);
  if (index >= leaf_page->GetSize()) {
    page_id_t next_page_id = leaf_page->GetNextPageId();
    bpm_->UnpinPage(curr_page_id, false); // 提前 Unpin 当前页
    
    if (next_page_id == INVALID_PAGE_ID) {
      // 如果没有下一页，说明找的 key 比树里所有元素都大，直接返回 End()
      return Iterator(INVALID_PAGE_ID, 0, bpm_);
    } else {
      //如果还有下一页，迭代器指向下一页的开头
      return Iterator(next_page_id, 0, bpm_); 
    }
  }

  // int index = leaf_page->KeyIndex(key, comparator_);
  bpm_->UnpinPage(curr_page_id, false);
  return Iterator(curr_page_id, index, bpm_);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto BPLUSTREE_TYPE::End() -> Iterator {
  return Iterator(INVALID_PAGE_ID, 0);
}

}  // namespace onebase
