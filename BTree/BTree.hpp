#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <cstdio>
#include <cstring>

namespace sjtu {
    typedef long PTR_FILE_CONT;///说实话这个我真的不知道应该设置为哪种数据类型。
    const int IOnum = 4096;
    //目前选择写在一个文件里面吧，这样的IO次数可能也不多。
    char EMPTY_NAME_FILE[10] = "my.dat";///还有这个，，文件命名应该怎么命名啊
    const size_t PTR_FILE_SIZE = sizeof(PTR_FILE_CONT);
    const size_t MAX_NUM_STACK = 10;

//#define DEBUGGING
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    private:
        struct data_index;
        struct data_leaf;
#ifdef DEBUGGING
        public:
#else
    private:
#endif
        static const size_t MAX_NUM_INDEX = (IOnum - 3 * PTR_FILE_SIZE - sizeof(short)
                                             - sizeof(char)) / sizeof(data_index);
        static const size_t MIN_NUM_INDEX = MAX_NUM_INDEX>>1;
        static const size_t MAX_NUM_LEAF = (IOnum - 3 * PTR_FILE_SIZE - sizeof(short)
                                            - sizeof(char)) / sizeof(data_leaf);
        static const size_t MIN_NUM_LEAF = MAX_NUM_LEAF>>1;
    private:
        ///所有的拷贝构造都没写，蛤。
        struct data_index{
            PTR_FILE_CONT next;
            Key keydata;
        };
        struct data_leaf{
            Key keydata;
            Value valdata;
        };
        //这里的curnum意思是key的个数，
        // 它的最大数目为MAX_NUM_INDEX - 2，最前面的用来储存最小的指针，留一个空的用来插入
        struct Node_index{
            char type;
            PTR_FILE_CONT myself;//不知道这个到底有没有必要？？？
            PTR_FILE_CONT next,prev;
            short curnum;
            data_index index[MAX_NUM_INDEX];
            Node_index():type('i'),curnum(0),myself(-1),next(-1),prev(-1){};
        };
        struct Node_leaf{
            char type;
            PTR_FILE_CONT myself;//不知道这个到底有没有必要？？？
            PTR_FILE_CONT next,prev;
            short curnum;
            data_leaf leaf[MAX_NUM_LEAF];
            Node_leaf():type('l'),curnum(0),myself(-1),next(-1),prev(-1){};
        };
        //root:最初始的值就是紧接着BPT之后的一个4k块，之后的话就不好说了，可以随便指向哪里
        //endoffset_file:文件的最后一个byte的offset,文件空的时候为0.
        //firstleaf：最初始不就是root嘛？
        ///等等，这里的初始值似乎有点问题。应该等第一个块插进来之后再去modify root和firstleaf的值。
        struct BPT{
            PTR_FILE_CONT root;
            PTR_FILE_CONT firstleaf;
            PTR_FILE_CONT endoffset_file;
            size_t sumnum_data;
            int height;//这里height是包含叶子那一层的。
            BPT():root(-1),firstleaf(-1),endoffset_file(0),sumnum_data(0),height(0){};
        };

        class myStack{
        private:
            Node_index **index;
            size_t curnum;
            size_t curmax;
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
            myStack():curnum(0),curmax(MAX_NUM_STACK){
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

#ifdef DEBUGGING
        public:
#else
    private:
#endif
        ///下面这些部分到底谁该public呢？
        //IO members
        char buffer[IOnum];
        FILE* ptr_file;
        char name[50];
        bool isopen;
        //Basic info of BPTree
        BPT* BPlusTree;
        //以下是每一次修改树的操作需要用到的信息
        /**不知道这里的有没有必要写在类里面？如果能写在函数里面是最好。
         * 如果你准备删了他们的话，要同时删：
         * 构造函数
         */
        bool isbuildchain;
        myStack* indexstack;
        Node_index *tmpindex;//我想用栈！！并且自己写了一个，但是不清楚具体的用途怎样。
        Node_leaf *leaf,*tmpleaf;//不知道tmpleaf能否用上？？？
        ///不清楚是不是需要用到tmpkey?
#ifdef DEBUGGING
        public:
#else
    private:
#endif
        /**IO func members__reference
         * 我能够考虑的到的特殊情况全部都throw了。
         * IO_alloc(需要开辟空间的大小):开辟一块新的空间（即文件尾位置的更改），将文件指针指向新空间并且准备写入。
         * IO_init():打开/创立一个空的文件。并将BPT基本的初始化信息写入
         * IO_destruct():关闭文件。
         * IO_getchain(begin):从begin处读入4K块，如果是索引就进栈。无法判断叶子节点被写了几次。
         *                    返回值：1表示索引节点，2表示叶节点。
         * IO_getleaf():将leaf节点内容写入tmpleaf指针。
         * IO_getnodeBPT():读取BPT基本信息。
         * IO_write():两个重载，分别处理叶子节点和普通节点的写入。
         * IO_write_nodeBPT():写入BPT中的基本信息。
         */
        ///感觉getblock()会有危险，因为难以保证每次leaf用完之后会置空。等待改正。
        //allocator,as well as modify endoffset_file.
        inline PTR_FILE_CONT IO_alloc(PTR_FILE_CONT need = IOnum){
            PTR_FILE_CONT tmp = BPlusTree->endoffset_file;
            fseek(ptr_file,0,SEEK_END);
            BPlusTree->ndoffset_file += need;
            return tmp;
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
            IO_alloc(sizeof(BPT));
            IO_write_nodeBPT();
        };
        inline void IO_destruct(){
            if(isopen) {
                int tmp = fclose(ptr_file);
                if(tmp == EOF) throw "in IO_destruct:the file cannot be closed.";
                isopen = false;
            }else{
                printf("try to destruct the file when it's not open");
                ptr_file = fopen(name,"wb+");
                fclose(ptr_file);
            }
        }

        char IO_getchain(PTR_FILE_CONT beg){
            if(empty()) throw "in IO_getblock:the tree is empty";

            new_chain();
            isbuildchain = true;
            fseek(ptr_file,beg,SEEK_SET);
            fread(buffer, sizeof(char),IOnum,ptr_file);
            if(*buffer == 'i'){
                new_tmpindex();
                memcpy(tmpindex,buffer,sizeof(Node_index));
                indexstack->push(tmpindex);
                return '1';
            }else if(*buffer == 'l'){
                memcpy(leaf,buffer,sizeof(Node_leaf));
                return '2';
            }
            if(*buffer == 'd')
                throw "in IO_getblock:this block has been deleted...";
            throw "in IO_getblock:invalid buffer block!!!Oh NO!!!";
        }
        void IO_getleaf(PTR_FILE_CONT beg){
            if(empty()) throw "in IO_getleaf:the tree is empty";

            fseek(ptr_file,beg,SEEK_SET);
            fread(buffer, sizeof(char),IOnum,ptr_file);
            if(*buffer == 'l'){
                new_tmpleaf();
                memcpy(tmpleaf,buffer,sizeof(Node_leaf));
                return;
            }
            if(*buffer == 'i') throw "in IO_getleaf:you get a index";
            if(*buffer == 'd') throw "in IO_getleaf:this leaf has been deleted...";
            throw "in IO_getleaf:invalid leafnode";
        }
        inline void IO_getnodeBPT(){
            fseek(ptr_file,0,SEEK_SET);
            fread(buffer,sizeof(BPT),1,ptr_file);
            memcpy(&BPlusTree,buffer,sizeof(BPT));
        }

        void IO_write(Node_leaf* tmpl){
            if(tmpl == nullptr)throw "in IO_write_leaf:the leaf is null= =";
            if(!(tmpl->type == 'l'||tmpl->type == 'd'))
                throw "in IO_write_leaf:invalid leafnode= =";

            memcpy(tmpl,buffer, sizeof(Node_leaf));
            fseek(ptr_file,tmpl->myself,SEEK_SET);
            int tmp = fwrite(buffer, sizeof(char),IOnum,ptr_file);
            if(!tmp) throw "in IO_write_leaf:cannot fwrite";
            fflush(ptr_file);
        };
        void IO_write(Node_index* tmpi){
            if(tmpi == nullptr)throw "in IO_write_index:the index is null= =";
            if(!(tmpi->type == 'i'||tmpi->type == 'd'))
                throw "in IO_write_index:invalid indexnode= =";

            memcpy(tmpi,buffer, sizeof(Node_index));
            fseek(ptr_file,tmpi->myself,SEEK_SET);
            int tmp = fwrite(buffer, sizeof(char),IOnum,ptr_file);
            if(!tmp) throw "in IO_write_index:cannot fwrite";
            fflush(ptr_file);
        };
        inline void IO_write_nodeBPT(){
            int tmp = fwrite(&BPlusTree, sizeof(BPT),1,ptr_file);
            if(!tmp) throw "in IO_write_nodeBPT:cannot write node BPT";
            fflush(ptr_file);
        }
#ifdef DEBUGGING
        public:
#else
    private:
#endif
        /**all short func__reference
         * new_tmpleaf():清除tmpleaf中的原有内容，建立一个新的tmpleaf,以防止内存泄漏。
         * new_tmpindex()：同上。
         * new_chain():准备从文件里读取一个新的链。
         * destroy_chain():在每一次修改树之后都要进行的操作。
         */
        inline void new_tmpleaf(){
            if(tmpleaf != nullptr){
                Node_leaf* del = tmpleaf;
                delete del;
            }
            tmpleaf = new Node_leaf;
        }
        inline void new_tmpindex(){
            if(tmpindex != nullptr){
                Node_index* del = tmpindex;
                delete del;
            }
            tmpindex = new Node_index;
        }
        inline void new_chain(){
            if(indexstack != nullptr){
                myStack *del = indexstack;
                delete del;
            }
            indexstack = new myStack;

            if(leaf != nullptr){
                Node_leaf* del = leaf;
                delete del;
            }
            leaf = new Node_leaf;
        }
        inline void destroy_chain(){
            if(indexstack != nullptr){
                myStack *del = indexstack;
                delete del;
            }
            indexstack = nullptr;

            if(leaf != nullptr){
                Node_leaf* del = leaf;
                delete del;
            }
            leaf = nullptr;
        }
        /**end func members__reference
         * End_getleaf():用于获得最后的叶子节点。
         */
        inline void End_getleaf(){
            IO_getleaf(BPlusTree->firstleaf);
            while(tmpleaf->next != -1){
                PTR_FILE_CONT tmpnext = tmpleaf->next;
                IO_getleaf(tmpnext);
            }
        }
        /**insert func members__reference
         *
         */

        /**find func members__reference
         * Find_seek_index()查找index中块的相对位置，返回值为下一级索引或着叶子的索引。
         * Find_seek_leaf()查找leaf中具体数据的位置，返回偏移量。如果没有相应key的话返回-1.
         *
         */
        inline PTR_FILE_CONT Find_seek_index(Node_index* index,const Key& findingkey){
            if(index == nullptr) throw "in F_seek_index:index not exist";

            for(short i = 1;i <= index->curnum;++i)
                if(findingkey < index->index[i].keydata) return index->index[i - 1].next;
            return index->index[index->curnum].next;
        }
        inline short Find_seek_leaf(Node_leaf* leaf,const Key& findingkey){
            if(leaf == nullptr) throw "in F_seek_leaf:leaf not exist";

            for(short i = 0;i < leaf->curnum;++i){
                if(leaf->leaf[i].keydata == findingkey) return i;
            }
            return -1;
        }
        inline void Find_getchain(const Key& key){
            char stat = IO_getblock(BPlusTree->root);
            while(stat == '1'){
                PTR_FILE_CONT tmpnext = Find_seek_index(tmpindex,key);
                stat = IO_getchain(tmpnext);
            }
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
            short pos_in_leafnode;//0-Based
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
        BTree():ptr_file(nullptr),isopen(false),isbuildchain(false),
                tmpindex(nullptr),tmpleaf(nullptr),leaf(nullptr),indexstack(nullptr){
            BPlusTree = new BPT;
            name = strcpy(name,EMPTY_NAME_FILE);
            IO_init();
        }
        BTree(const BTree& other) {
            // Todo Copy
        }
        BTree& operator=(const BTree& other) {
            // Todo Assignment
        }
        ~BTree() {
            IO_destruct();
            if(BPlusTree != nullptr)delete BPlusTree;
            if(BPlusTree != nullptr)delete indexstack;
            if(leaf != nullptr)delete leaf;
            if(tmpleaf != nullptr)delete tmpleaf;
            if(tmpindex != nullptr)delete tmpindex;
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
        iterator begin() {
            if(empty()){
                //以下这个if表示对firstleaf的完全信任。
                if(BPlusTree->root == -1){
                    new_tmpleaf();
                    tmpleaf->myself = IO_alloc(IOnum);
                    BPlusTree->root = BPlusTree->firstleaf = tmpleaf->myself;
                    IO_write(tmpleaf);
                }
            }
            return iterator(this,BPlusTree->firstleaf,0);
        }
        const_iterator cbegin() const {
            if(empty()){
                //以下这个if表示对firstleaf的完全信任。
                if(BPlusTree->root == -1){
                    new_tmpleaf();
                    tmpleaf->myself = IO_alloc(IOnum);
                    BPlusTree->root = BPlusTree->firstleaf = tmpleaf->myself;
                    IO_write(tmpleaf);
                }
            }
            return const_iterator(this,BPlusTree->firstleaf,0);
        }
        // Return a iterator to the end(the next element after the last)
        ///如果是空树的话到底返回什么呢？
        iterator end() {
            if(empty()){
                //以下这个if表示对firstleaf的完全信任。
                if(BPlusTree->root == -1){
                    new_tmpleaf();
                    tmpleaf->myself = IO_alloc(IOnum);
                    BPlusTree->root = BPlusTree->firstleaf = tmpleaf->myself;
                    IO_write(tmpleaf);
                }
                return iterator(this,BPlusTree->firstleaf,0);
            }else{
                End_getleaf();
                return iterator(this,tmpleaf->myself,tmpleaf->curnum);
            }
        }
        const_iterator cend() const {
            if(empty()){
                //以下这个if表示对firstleaf的完全信任。
                if(BPlusTree->root == -1){
                    new_tmpleaf();
                    tmpleaf->myself = IO_alloc(IOnum);
                    BPlusTree->root = BPlusTree->firstleaf = tmpleaf->myself;
                    IO_write(tmpleaf);
                }
                return const_iterator(this,BPlusTree->firstleaf,0);
            }else{
                End_getleaf();
                return const_iterator(this,tmpleaf->myself,tmpleaf->curnum);
            }
        }
        // Check whether this BTree is empty
        //use BPT::sumnum_Data
        bool empty() const {
            if(BPlusTree->sumnum_data == 0) return true;
            return false;
        }
        // Return the number of <K,V> pairs
        size_t size() const { return BPlusTree->sumnum_data;}
        // Clear the BTree
        void clear() {
            IO_destruct();
            BPT* tmpbpt = BPlusTree;BPlusTree = new BPT;if(tmpbpt != nullptr)delete tmpbpt;
            myStack* tmpstack = indexstack;indexstack = new myStack;
            if(tmpstack != nullptr)delete indexstack;
            if(leaf != nullptr){
                delete leaf;
                leaf = nullptr;
            }
            if(tmpleaf != nullptr){
                delete tmpleaf;
                tmpleaf = nullptr;
            }
            if(tmpindex != nullptr){
                delete tmpindex;
                tmpindex = nullptr;
            }
            IO_init();
        }
        // Return the value refer to the Key(key)
        Value at(const Key& key){
            const_iterator tmp = find(key);

        }
        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key) const {

        }
        /**
         * Finds an element with key equivalent to key.
         * key value of the element to search for.
         * Iterator to an element with key equivalent to key.
         *   If no such element is found, past-the-end (see end()) iterator is
         * returned.
         */
        ///问题：如果是空树应该怎么办呢？
        iterator find(const Key& key) {
            if(empty()) throw "in find1:try to find in an empty tree";
            short pos;
            if(isbuildchain) {
                if(key <= leaf->leaf[leaf->curnum - 1].keydata
                   &&key >= leaf->leaf[0].keydata){
                    pos = Find_seek_leaf(leaf,key);
                    if(pos != -1) return iterator(this,leaf->myself,pos);
                    else return end();
                }
            }
            Find_getchain(key);
            pos = Find_seek_leaf(leaf,key);
            if(pos != -1) return iterator(this,leaf->myself,pos);
            else return end();
        }
        const_iterator find(const Key& key) const {
            if(empty()) throw "in find1:try to find in an empty tree";
            short pos;
            if(isbuildchain) {
                if(key <= leaf->leaf[leaf->curnum - 1].keydata
                   &&key >= leaf->leaf[0].keydata){
                    pos = Find_seek_leaf(leaf,key);
                    if(pos != -1) return const_iterator(this,leaf->myself,pos);
                    else return end();
                }
            }
            Find_getchain(key);
            pos = Find_seek_leaf(leaf,key);
            if(pos != -1) return const_iterator(this,leaf->myself,pos);
            else return end();
        }
    };
}  // namespace sjtu