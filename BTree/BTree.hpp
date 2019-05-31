#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <cstdio>

namespace sjtu {
#define NONEX_FILE nullptr;
    typedef long PTR_FILE_CONT;//说实话这个我真的不知道应该设置为哪种数据类型。
    const int IOnum = 4096;

    //目前选择写在一个文件里面吧，这样的IO次数可能也不多。
//#define IS_STRUCTURING_IO_index
#define IS_STRUCTURING_IO_content
    const char EMPTY_NAME_FILE = '\0';
    class IO_index{
        //data members
#ifdef IS_STRUCTURING_IO_index
        public:
#else
    private:
#endif
        FILE* ptr_file = NONEX_FILE;
        char name[55] = {EMPTY_NAME_FILE};
        PTR_FILE_CONT endoffset_file = 0;
        bool isopen = false;

        //func members
        void IO_init(){};
        void IO_getblock(char*){};//<=>read(),用学长说过的读入优化
        void IO_write(){};
        void IO_open(){};
        void IO_close(){};
        void IO_clear(){};
        bool isOpen(){
            return isopen;
        }
    };
    /*class IO_content{
#ifdef IS_STRUCTURING_IO_content
        public:
#else
    private:
#endif
        FILE* ptr_file = NONEX_FILE;
        char name[55] = {EMPTY_NAME_FILE};
        PTR_FILE_CONT endoffset_file = 0;
        bool isopen = false;
    };*/

#define IS_STRUCTURING_BPT
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
#ifdef IS_STRUCTURING_BPT
    public:
#else
        private:
#endif
        //const size_t
        // Your private members go here
#ifdef IS_STRUCTURING_BPT
    public:
#else
        private:
#endif
        IO_index io;//???
        char *buffer = new char[IOnum];

    public:

    public:
        typedef pair<const Key, Value> value_type;


        class const_iterator;
        class iterator {
        private:
            // Your private members go here
        public:
            bool modify(const Value& value){

            }
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

        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        OperationResult erase(const Key& key) {
            // TODO erase function
            return Fail;  // If you can't finish erase part, just remaining here.
        }
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
        // Return the value refer to the Key(key)
        Value at(const Key& key){
        }
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