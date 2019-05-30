#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
namespace sjtu {
//#define _NO_DEBUG
    typedef int PTR_infile;//？？我实在不清楚指针到底使用什么数据类型，所以就用这个来代替吧
    typedef FILE* PTR_file;//容易混淆，目前使用FILE去定义
    typedef size_t Numtype;
    static const Numtype MAX_OF_INDEX = 256;//实在不清楚应有大小
    static const Numtype MAX_OF_LEAF = 256;//同上
    static const Numtype MAX_OF_FILENAME = 256;
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
        //内存里记录的数据。
#ifndef _NO_DEBUG
    private:
#else
        public:
#endif
        mutable bool isFileOpen = false;
        mutable FILE* file_now = nullptr;
        const char name_file[MAX_OF_FILENAME];
        int height = 0;
        //节点设置
    public://？？？不清楚应该是什么
        //它应该始终放在文件首
        struct BPT{
            PTR_infile root;
            PTR_infile firstleafnode;
            Numtype sumnum_data;
            //PTR_infile end_file = SEEK_END;
        };
#ifndef _NO_DEBUG
    private:
#else
        public:
#endif
        // Your private members go here
        struct Data_common{
            Key key;
            PTR_infile child;
        };
        struct Data_leaf{
            Key key;
            Value data;
        };
        struct Node_common{
            PTR_infile father;
            PTR_infile prev,next;
            Data_common index[MAX_OF_INDEX];
            Numtype curnum_comon;
        };
        struct Node_leaf{
            PTR_infile father;
            PTR_infile prev,next;
            Data_leaf leaf[MAX_OF_LEAF];
            Numtype curnum_leaf;
        };
        //工具函数集合
#ifndef _NO_DEBUG
    private:
#else
        public:
#endif
        /**
         * 这里用来声明（以及实现）所有IO相关的函数，一定要注意它的可移植性
         * 建树、建立节点相关函数，用在构造函数里面调用setroot,
         * 用于构造函数以及分裂到根节点之后重新建树
         * allocate，用于定位到文件最后，给人一种分配空间的想法
         */
        //Setroot,最初建树和分裂根节点都可以用，三个参数第一个true就是建空树。
        void Setroot(bool isNewBPT = true,
                     PTR_infile root_former = -1,PTR_infile root_newchild = -1){

        };
        //读取相关函数，从文件中取到内存里面
        //这个估计就是用来构造函数
        void IO_iniBPT(){
            if(file_now != nullptr) {
                fclose(file_now);
                fopen(name_file,"wb+");
                fclose(file_now);
            }
            fopen(name_file,"rb+");

        }
        /*reach_from_root
         *
         */
    public:
        typedef pair<const Key, Value> value_type;

  class const_iterator;
  class iterator {
   private:
    // Your private members go here
   public:
    iterator() {
      // TODO Default Constructor
    }
    iterator(const iterator& other) {
      // TODO Copy Constructor
    }
    // Return a new iterator which points to the n-next elements
    iterator operator++(int) {
      // Todo iterator++
    }
    iterator& operator++() {
      // Todo ++iterator
    }
    iterator operator--(int) {
      // Todo iterator--
    }
    iterator& operator--() {
      // Todo --iterator
    }
    // Overloaded of operator '==' and '!='
    // Check whether the iterators are same
    value_type& operator*() const {
      // Todo operator*, return the <K,V> of iterator
    }
    bool operator==(const iterator& rhs) const {
      // Todo operator ==
    }
    bool operator==(const const_iterator& rhs) const {
      // Todo operator ==
    }
    bool operator!=(const iterator& rhs) const {
      // Todo operator !=
    }
    bool operator!=(const const_iterator& rhs) const {
      // Todo operator !=
    }
    value_type* operator->() const noexcept {
      /**
       * for the support of it->first.
       * See
       * <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/>
       * for help.
       */
    }
  };
  class const_iterator {
    // it should has similar member method as iterator.
    //  and it should be able to construct from an iterator.
   private:
    // Your private members go here
   public:
    const_iterator() {
      // TODO
    }
    const_iterator(const const_iterator& other) {
      // TODO
    }
    const_iterator(const iterator& other) {
      // TODO
    }
    // And other methods in iterator, please fill by yourself.
  };
  // Default Constructor and Copy Constructor
  BTree() {
    // Todo Default
  }
  BTree(const BTree& other) {
    // Todo Copy
  }
  BTree& operator=(const BTree& other) {
    // Todo Assignment
  }
  ~BTree() {
    // Todo Destructor
  }
  // Insert: Insert certain Key-Value into the database
  // Return a pair, the first of the pair is the iterator point to the new
  // element, the second of the pair is Success if it is successfully inserted
  pair<iterator, OperationResult> insert(const Key& key, const Value& value) {
    // TODO insert function
  }
  // Erase: Erase the Key-Value
  // Return Success if it is successfully erased
  // Return Fail if the key doesn't exist in the database
  OperationResult erase(const Key& key) {
    // TODO erase function
    return Fail;  // If you can't finish erase part, just remaining here.
  }
  // Overloaded of []
  // Access Specified Element
  // return a reference to the first value that is mapped to a key equivalent to
  // key. Perform an insertion if such key does not exist.
  Value& operator[](const Key& key) {}
  // Overloaded of const []
  // Access Specified Element
  // return a reference to the first value that is mapped to a key equivalent to
  // key. Throw an exception if the key does not exist.
  const Value& operator[](const Key& key) const {}
  // Access Specified Element
  // return a reference to the first value that is mapped to a key equivalent to
  // key. Throw an exception if the key does not exist
  Value& at(const Key& key) {}
  // Overloaded of const []
  // Access Specified Element
  // return a reference to the first value that is mapped to a key equivalent to
  // key. Throw an exception if the key does not exist.
  const Value& at(const Key& key) const {}
  // Return a iterator to the beginning
  iterator begin() {}
  const_iterator cbegin() const {}
  // Return a iterator to the end(the next element after the last)
  iterator end() {}
  const_iterator cend() const {}
  // Check whether this BTree is empty
  bool empty() const {}
  // Return the number of <K,V> pairs
  size_t size() const {}
  // Clear the BTree
  void clear() {}
  /**
   * Returns the number of elements with key
   *   that compares equivalent to the specified argument,
   * The default method of check the equivalence is !(a < b || b > a)
   */
  size_t count(const Key& key) const {}
  /**
   * Finds an element with key equivalent to key.
   * key value of the element to search for.
   * Iterator to an element with key equivalent to key.
   *   If no such element is found, past-the-end (see end()) iterator is
   * returned.
   */
  iterator find(const Key& key) {}
  const_iterator find(const Key& key) const {}
};
}  // namespace sjtu