#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <cstdio>
#include <cstring>

namespace sjtu {
    typedef long PTR_FILE_CONT;//说实话这个我真的不知道应该设置为哪种数据类型。
    const int IOnum = 4096;
    //目前选择写在一个文件里面吧，这样的IO次数可能也不多。
    const char EMPTY_NAME_FILE = '\0';
    const size_t CHAR_SIZE = sizeof(char);
    const size_t PTR_FILE_SIZE = sizeof(PTR_FILE_CONT);
    const size_t MAX_NUM_STACK = 100;

#define IS_STRUCTURING_BPT
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    private:
        struct data_index;
        struct data_leaf;
#ifdef IS_STRUCTURING_BPT
    public:
#else
        private:
#endif
        static const size_t MAX_NUM_INDEX = (IOnum - 4 * PTR_FILE_SIZE - sizeof(short)
                                             - sizeof(char)) / sizeof(data_index);
        static const size_t MIN_NUM_INDEX = MAX_NUM_INDEX>>1;
        static const size_t MAX_NUM_LEAF = (IOnum - 3 * PTR_FILE_SIZE - sizeof(short)
                                            - sizeof(char)) / sizeof(data_leaf);
        static const size_t MIN_NUM_LEAF = MAX_NUM_LEAF>>1;
        // Your private members go here
    private:
        struct data_index{
            PTR_FILE_CONT prev;
            Key keydata;
        };
        struct data_leaf{
            Key keydata;
            Value valdata;
        };
        struct Node_index{
            const char type = 'i';
            PTR_FILE_CONT myself;//不知道这个到底有没有必要？？？
            PTR_FILE_CONT next,prev;
            PTR_FILE_CONT maxchild;
            short curnum = 0;
            data_index index[MAX_NUM_INDEX];
        };
        struct Node_leaf{
            const char type = 'l';
            PTR_FILE_CONT myself;//不知道这个到底有没有必要？？？
            PTR_FILE_CONT next,prev;
            short curnum = 0;
            data_leaf leaf[MAX_NUM_LEAF];
        };
        //root:最初始的值就是紧接着BPT之后的一个4k块，之后的话就不好说了，可以随便指向哪里
        //endoffset_file:文件的最后一个byte的offset,文件空的时候为0.
        //firstleaf：最初始不就是root嘛？
        //等等，这里的初始值似乎有点问题。应该等第一个块插进来之后再去modify root和firstleaf的值。
        struct BPT{
            PTR_FILE_CONT root = sizeof(BPT);
            PTR_FILE_CONT firstleaf = sizeof(BPT);
            PTR_FILE_CONT endoffset_file = 0;
            long sumnum_data = 0;
            int height = 0;
        };

        class myStack{
        private:
            Node_index **index;
            size_t curnum = 0;
            size_t curmax = MAX_NUM_STACK;
            void doublespace(){
                Node_index** del = index;
                index = new Node_index*[curmax * 2];
                for(size_t i = 0;i < curnum;++i)
                    index[i] = del[i];
                curmax *= 2;
                for(size_t i = curnum;i < curmax;++i)
                    index[i] = nullptr;
                delete []del;
            }
        public:
            myStack():curmax(MAX_NUM_STACK){
                index = new Node_index*[curmax];
                for(int i = 0;i < curmax;++i)
                    index[i] = nullptr;
            }
            void push(Node_index* tmpindex){
                if(curnum == curmax)doublespace();
                if(!index[curnum]) index[curnum] = new Node_index;
                *(index[curnum]) = *(tmpindex);
                ++curnum;
            }
            Node_index pop(){
                --curnum;
                Node_index tmp = *(index[curnum]),*del = index[curnum];
                index[curnum] = nullptr;
                delete del;
                return tmp;
            }
            void clear(){
                Node_index* del;
                for(int i = 0;i < curnum;++i){
                    del = index[i];
                    index[i] = nullptr;
                    delete del;
                }
                curnum = 0;
            }
            ~myStack(){
                for(int i = 0;i < curnum;++i)
                    delete index[i];
                delete []index;
            }
        };

    public://说实话感觉这里面有些东西不应该是public，写完再说
        char *buffer = new char[IOnum];
        BPT* BPlusTree = new BPT;
        myStack indexstack;
        Node_index *tmpindex = nullptr;//我想用栈！！！！！！！懒人不想自己写。没错。
        Node_leaf *leaf = nullptr,*tmpleaf = nullptr;//不知道tmpleaf能否用上？？？
        //IO members
        FILE* ptr_file = nullptr;
        char name[55] = {EMPTY_NAME_FILE};
        bool isopen = false;

        //IO func members
        /*
         * 关于cstdio中函数的使用说明：
         *fopen：开文件时一定得左边放一个FILE指针。
         *在每次fread之前一定得来个fseek,并且一定要seek到要读的一位之前。
         *   例：SEEK_SET等价于0，read出来就是从1开始读。
         *每次都读出定长，就可以避免出现：有一次比之前读的少，但是字符串长度未改变的尴尬情形。
         */
        //allocator,as well as modify endoffset_file.
        inline PTR_FILE_CONT alloc(PTR_FILE_CONT need){
            PTR_FILE_CONT tmp = BPlusTree->endoffset_file;
            fseek(ptr_file,0,SEEK_END);
            BPlusTree->endoffset_file += need;
            return tmp;
        };
        //这个应该理解为删除吧？？？
        void dealloc(){

        };
        //仅供default构造函数使用。没有写复制构造相关的。
        void IO_init(){
            if(isopen){
                if(!fclose(ptr_file)) throw "in IO_init:the file cannot be renewed.";
                ptr_file = fopen(name,"wb+");
                if(ptr_file == nullptr)throw "in IO_init:the file cannot be initialed.";
            }else{
                ptr_file = fopen(name,"wb+");
                if(ptr_file == nullptr)throw "in IO_init:the file cannot be initialed.";
                isopen = true;
            }
            alloc(sizeof(BPT));
            int tmp = fwrite(BPlusTree, sizeof(BPT),1,ptr_file);
            if(!tmp) throw "in IO_init:cannot write node BPT";
            fflush(ptr_file);
        };
        //0表示空树，1表示普通节点，2表示叶节点
        //<=>read(),用学长说过的读入优化
        char IO_getblock(PTR_FILE_CONT beg){
            if(BPlusTree->sumnum_data == 0)
                return '0';
            fseek(ptr_file,beg,SEEK_SET);
            fread(buffer, sizeof(char),IOnum,ptr_file);
            if(*buffer == 'i'){
                if(tmpindex == nullptr) tmpindex = new Node_index;
                memcpy(tmpindex,buffer,sizeof(Node_index));
                indexstack.push(tmpindex);
                return '1';
            }else if(*buffer == 'l'){
                if(leaf == nullptr) leaf = new Node_leaf;
                memcpy(leaf,buffer,sizeof(Node_leaf));
                return '2';
            }
            if(*buffer == 'd')
                throw "in IO_getblock:this block has been deleted...";
            throw "in IO_getblock:invalid buffer block!!!Oh NO!!!";
        }

        inline void IO_write(Node_leaf* tmpl){
            if(tmpl == nullptr)throw "in IO_write_leaf:the leaf is null= =";
            if(!(tmpl->type == 'l'||tmpl->type == 'd'))
                throw "in IO_write_leaf:invalid leafnode= =";

            memcpy(tmpl,buffer, sizeof(Node_leaf));
            fseek(ptr_file,tmpl->myself,SEEK_SET);
            int tmp = fwrite(buffer, sizeof(char),IOnum,ptr_file);
            if(!tmp) throw "in IO_write_leaf:cannot fwrite";
        };
        inline void IO_write(Node_index* tmpi){
            if(tmpi == nullptr)throw "in IO_write_index:the index is null= =";
            if(!(tmpi->type == 'i'||tmpi->type == 'd'))
                throw "in IO_write_index:invalid indexnode= =";

            memcpy(tmpi,buffer, sizeof(Node_index));
            fseek(ptr_file,tmpi->myself,SEEK_SET);
            int tmp = fwrite(buffer, sizeof(char),IOnum,ptr_file);
            if(!tmp) throw "in IO_write_index:cannot fwrite";
        };

        void IO_open(){};
        void IO_close(){};
        void IO_clear(){};

    public:
        //和上面的getblock相对应。
        inline PTR_FILE_CONT seekblock_index(Node_index* index,Key findingkey){
            if(index == nullptr) throw "in seekblock_index:index not exist";
            for(int i = 0;i < index->curnum;++i)
                if(findingkey < index->index[i].keydata) return index->index[i].prev;
            return index->maxchild;
        }
    public:
        typedef pair<const Key, Value> value_type;

        class const_iterator;
        class iterator {
            friend class BTree;
        private:
            // Your private members go here
            BTree* Tree;
            PTR_FILE_CONT offset_leafnode;//记录所在叶子节点的块
            short pos_in_leafnode;
        public:
            bool modify(const Value& value){

            }
            iterator() {
                Tree = nullptr;//why?是不是因为它读不成自己所在的树
                offset_leafnode = 0;
                pos_in_leafnode = -1;//为了防止重叠就将空的设为1趴
            }
            iterator(BTree* t,PTR_FILE_CONT o,short p)
                    :Tree(t),offset_leafnode(o),pos_in_leafnode(p)
            {};
            iterator(const iterator& other) {
                Tree = other.Tree;
                offset_leafnode = other.offset_leafnode;
                pos_in_leafnode = other.pos_in_leafnode;
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
            BTree* Tree;
            PTR_FILE_CONT offset_leafnode;//记录所在叶子节点的块
            int pos_in_leafnode;
        public:
            const_iterator() {
                Tree = nullptr;//why?是不是因为它读不成自己所在的树
                offset_leafnode = 0;
                pos_in_leafnode = -1;//为了防止重叠就将空的设为1趴
            }
            const_iterator(const const_iterator& other) {
                Tree = other.Tree;
                offset_leafnode = other.offset_leafnode;
                pos_in_leafnode = other.pos_in_leafnode;
            }
            const_iterator(const iterator& other) {
                Tree = other.Tree;
                offset_leafnode = other.offset_leafnode;
                pos_in_leafnode = other.pos_in_leafnode;
            }
            const_iterator(BTree* t,PTR_FILE_CONT o,short p)
                    :Tree(t),offset_leafnode(o),pos_in_leafnode(p)
            {};
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