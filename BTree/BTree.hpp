#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <cstdio>
#include <cstring>

namespace sjtu {
//#define DEBUGPROCESS
    typedef long PTR_FILE_CONT;///说实话这个我真的不知道应该设置为哪种数据类型。
#ifdef DEBUGPROCESS
    const int IOnum = 55;
#else
    const int IOnum = 4096;
#endif
    //目前选择写在一个文件里面吧，这样的IO次数可能也不多。
    char EMPTY_NAME_FILE[10] = "my.dat";///还有这个，，文件命名应该怎么命名啊
    const size_t MAX_NUM_STACK = 15;

//#define DEBUGGING
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    private:
        struct data_index;
        struct data_leaf;
    private:
        static const size_t MAX_NUM_INDEX = (IOnum - 3 * sizeof(PTR_FILE_CONT) - sizeof(short)
                                             - sizeof(char) - sizeof(data_index*)) / sizeof(data_index);
        static const size_t MIN_NUM_INDEX = MAX_NUM_INDEX>>1;
        static const size_t MAX_NUM_LEAF = (IOnum - 3 * sizeof(PTR_FILE_CONT) - sizeof(short)
                                            - sizeof(char) - sizeof(data_leaf*)) / sizeof(data_leaf);
        static const size_t MIN_NUM_LEAF = MAX_NUM_LEAF>>1;
    private:
        //经过实验，节点类全都不需要构造函数和赋值重载。
        //这充分证明了我程设学的有多不扎实。
        struct data_index{
            PTR_FILE_CONT next;
            Key keydata;
        };
        struct data_leaf{
            Key keydata;
            Value valdata;
        };
        //这里的curnum意思是key的个数，
        //它的最大数目为MAX_NUM_INDEX - 2，最前面的用来储存最小的指针，留一个空的用来插入
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
        struct BPT{
            PTR_FILE_CONT root;
            PTR_FILE_CONT firstleaf;
            PTR_FILE_CONT endoffset_file;
            size_t sumnum_data;
            int height;//这里height是包含叶子那一层的。
            BPT():root(-1),firstleaf(-1),endoffset_file(0),sumnum_data(0),height(0){};
        };

        class myStack{
            friend class BTree;
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
            bool isStackempty(){return (curnum == 0);}
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
        //IO members
        char buffer[IOnum];
        FILE* ptr_file;
        char name[50];
        bool isfexist;
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
         * IO_getindex():将index节点内容写入tmpindex指针。返回值：'i'表示正常情况，'l'表示读到叶节点。
         * IO_getnodeBPT():读取BPT基本信息。
         * IO_write():两个重载，分别处理叶子节点和普通节点的写入。
         * IO_write_nodeBPT():写入BPT中的基本信息。
         */
        //allocator,as well as modify endoffset_file.
        inline PTR_FILE_CONT IO_alloc(PTR_FILE_CONT need = IOnum){
            PTR_FILE_CONT tmp = BPlusTree->endoffset_file;
            fseek(ptr_file,tmp,SEEK_SET);
            BPlusTree->endoffset_file += need;
            return tmp;
        };
        //仅供default构造函数使用。没有写复制构造相关的。
        void IO_init(){
            ptr_file = fopen(name,"rb+");
            if(ptr_file == nullptr){
                ptr_file = fopen(name,"w+");
                fclose(ptr_file);
                ptr_file = fopen(name,"rb+");
                isfexist = false;
            }else isfexist = true;

            if(isfexist){
                IO_getnodeBPT();
            }else{
                IO_alloc(sizeof(BPT));
                IO_write_nodeBPT();
            }
        };
        inline void IO_destruct(){
            int tmp = fclose(ptr_file);

            if(tmp == EOF) throw "in IO_destruct:the file cannot be closed.";

        }

        char IO_getchain(PTR_FILE_CONT beg){
            if(empty()) throw "in IO_getblock:the tree is empty";

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
        char IO_getindex(PTR_FILE_CONT beg){
            if(empty()) throw "in IO_getindex:the tree is empty";

            fseek(ptr_file,beg,SEEK_SET);
            fread(buffer, sizeof(char),IOnum,ptr_file);
            if(*buffer == 'i'){
                new_tmpindex();
                memcpy(tmpindex,buffer,sizeof(Node_index));
                return 'i';
            }
            if(*buffer == 'l') return 'l';
            if(*buffer == 'd') throw "in IO_getindex:this index has been deleted...";
            throw "in IO_getindex:invalid indexnode";
        }
        inline void IO_getnodeBPT(){
            fseek(ptr_file,0,SEEK_SET);
            fread(buffer,sizeof(BPT),1,ptr_file);
            memcpy(BPlusTree,buffer,sizeof(BPT));
        }

        void IO_write(Node_leaf* tmpl){
            if(tmpl == nullptr)throw "in IO_write_leaf:the leaf is null= =";
            if(!(tmpl->type == 'l'||tmpl->type == 'd'))
                throw "in IO_write_leaf:invalid leafnode= =";

            memcpy(buffer,tmpl,sizeof(Node_leaf));
            fseek(ptr_file,tmpl->myself,SEEK_SET);
            int tmp = fwrite(buffer, sizeof(char),IOnum,ptr_file);
            if(!tmp) throw "in IO_write_leaf:cannot fwrite";
            fflush(ptr_file);
        }
        void IO_write(Node_index* tmpi){
            if(tmpi == nullptr)throw "in IO_write_index:the index is null= =";
            if(!(tmpi->type == 'i'||tmpi->type == 'd'))
                throw "in IO_write_index:invalid indexnode= =";

            memcpy(buffer,tmpi, sizeof(Node_index));
            fseek(ptr_file,tmpi->myself,SEEK_SET);
            int tmp = fwrite(buffer, sizeof(char),IOnum,ptr_file);
            if(!tmp) throw "in IO_write_index:cannot fwrite";
            fflush(ptr_file);
        };
        inline void IO_write_nodeBPT(){
            fseek(ptr_file,0,SEEK_SET);
            int tmp = fwrite(BPlusTree, sizeof(BPT),1,ptr_file);
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
         * destroy_chain():在每一次修改树之后都要进行的操作,将isbulidchain改为false。
         */
        inline void new_tmpleaf(){
            if(tmpleaf != nullptr){
                Node_leaf* del = tmpleaf;
                tmpleaf = new Node_leaf;
                delete del;
                return;
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
            isbuildchain = false;
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
         * INS_seek_leaf():用于查找插入key的相对位置。如果遇到相等的情况就返回-1。
         * INS_getchain():用于获得需要修改的一条链，每次获得会将之前获得的chain全部清除。
         *                 但是不能更改isbuildchain的值。
         * INS_find():返回具体比插入key略大的位置。
         * INS_inleaf():仅仅完成leaf节点的内部插入过程，修改curnum,sumnum.
         * INS_inindex():index节点的内部插入过程，查找使用指针来比较。修改了curnum。
         * INS_splitleaf():将chain中的叶节点分裂，并写入文件。传递了要push上去的值。
         * INS_splitindex():将chain中的索引节点分裂，并写入文件。传递了push上去的值。
         *                  吐槽一下curnum设置的有点狗血
         */
        short INS_seek_leaf(const Key& key)const{
            if(leaf == nullptr) throw "in INS_seek_leaf:leaf not exist";

            if(key < leaf->leaf[0].keydata) return 0;
            for(short i = 1;i < leaf->curnum;++i){
                if(key < leaf->leaf[i].keydata&&key > leaf->leaf[i - 1].keydata) return i;
            }
            if(key > leaf->leaf[leaf->curnum - 1].keydata) return leaf->curnum;

            return -1;
        }
        inline void INS_getchain(const Key& key){
            new_chain();
            char stat = IO_getchain(BPlusTree->root);
            while(stat == '1'){
                PTR_FILE_CONT tmpnext = Find_seek_index(tmpindex,key);
                stat = IO_getchain(tmpnext);
            }
        }
        short INS_find(const Key key) {
            if(empty()) throw "in INS_find:try to find in an empty tree";
            short pos;
            if(isbuildchain) {
                if(leaf->leaf[leaf->curnum - 1].keydata >= key
                   &&key >= leaf->leaf[0].keydata){
                    pos = INS_seek_leaf(key);
                    if(pos == -1) throw "in INS_find:find the same key";
                    return pos;
                }
            }
            else isbuildchain = true;

            INS_getchain(key);
            pos = INS_seek_leaf(key);
            if(pos == -1) throw "in INS_find:find the same key";
            return pos;
        }
        void INS_inleaf(const Key& key, const Value& value){
            short pos = INS_find(key),i = leaf->curnum;
            while(i > pos){
                leaf->leaf[i] = leaf->leaf[i - 1];
                --i;
            }
            leaf->leaf[pos].keydata = key;
            leaf->leaf[pos].valdata = value;
            ++leaf->curnum;
            ++BPlusTree->sumnum_data;
        }
        void INS_inindex(data_index &info_pushup,Node_index &index,PTR_FILE_CONT son){
            short pos = 0,i = index.curnum;
            //这里对下一行的存在性进行了信任。
            while (index.index[pos].next != son) ++pos;
            while(i > pos){
                index.index[i + 1] = index.index[i];
                --i;
            }
            index.index[pos + 1] = info_pushup;
            ++index.curnum;
        }
        void INS_splitleaf(data_index &info_pushup){
            new_tmpleaf();
            for(int i = (MAX_NUM_LEAF>>1);i < MAX_NUM_LEAF;++i)
                tmpleaf->leaf[i - (MAX_NUM_LEAF>>1)] = leaf->leaf[i];

            tmpleaf->next = leaf->next;
            tmpleaf->prev = leaf->myself;
            tmpleaf->curnum = MAX_NUM_LEAF - (MAX_NUM_LEAF>>1);
            tmpleaf->myself = IO_alloc();
            IO_write(tmpleaf);

            leaf->next = tmpleaf->myself;
            leaf->curnum = (MAX_NUM_LEAF>>1);
            IO_write(leaf);

            info_pushup.next = tmpleaf->myself;
            info_pushup.keydata = tmpleaf->leaf[0].keydata;
        }
        void INS_splitindex(data_index &info_pushup,Node_index &index){
            new_tmpindex();
            for(short i = (MAX_NUM_INDEX>>1);i < MAX_NUM_INDEX;++i)
                tmpindex->index[i - (MAX_NUM_INDEX>>1)] = index.index[i];

            tmpindex->next = index.next;
            tmpindex->prev = index.myself;
            tmpindex->curnum = MAX_NUM_INDEX - (MAX_NUM_INDEX>>1) - 1;
            tmpindex->myself = IO_alloc();
            IO_write(tmpindex);

            index.next = tmpindex->myself;
            index.curnum = (MAX_NUM_INDEX>>1) - 1;
            IO_write(&index);

            info_pushup.next = tmpindex->myself;
            info_pushup.keydata = tmpindex->index[0].keydata;
        }
        void INS_resetroot(data_index &info_pushup){
            new_tmpindex();
            tmpindex->index[0].next = BPlusTree->root;
            tmpindex->index[1] = info_pushup;
            tmpindex->curnum = 1;
            tmpindex->myself = IO_alloc();
            IO_write(tmpindex);

            ++BPlusTree->height;
            BPlusTree->root = tmpindex->myself;
            IO_write_nodeBPT();
        }
        /**find func members__reference
         * Find_seek_index()查找index中块的相对位置，返回值为下一级索引或着叶子的索引。
         * Find_seek_leaf()查找leaf中具体数据的位置，返回偏移量。如果没有相应key的话返回-1.
         * Find_getleaf():用于找到目标叶子节点，并且读入内存。但是进行此操作之后内存中未保存整个一条链。
         */
        inline PTR_FILE_CONT Find_seek_index(Node_index* index, const Key& findingkey)const{
            if(index == nullptr) throw "in F_seek_index:index not exist";

            if(findingkey < index->index[1].keydata) return index->index[0].next;
            for(short i = 2;i <= index->curnum;++i)
                if(findingkey < index->index[i].keydata&&findingkey >= index->index[i - 1].keydata)
                    return index->index[i - 1].next;
            return index->index[index->curnum].next;
        }
        inline short Find_seek_leaf(Node_leaf* leaf, const Key& findingkey)const{
            if(leaf == nullptr) throw "in F_seek_leaf:leaf not exist";

            for(short i = 0;i < leaf->curnum;++i){
                if(leaf->leaf[i].keydata == findingkey) return i;
            }
            return -1;
        }
        inline void Find_getleaf(const Key& key){
            PTR_FILE_CONT tmpnext = BPlusTree->root;
            char stat = IO_getindex(tmpnext);
            while(stat == 'i'){
                tmpnext = Find_seek_index(tmpindex,key);
                stat = IO_getindex(tmpnext);
            }
            if(stat == 'l') IO_getleaf(tmpnext);
        }
    public:
        typedef pair<const Key, Value> value_type;

        class const_iterator;
        class iterator {
            friend class BTree;
            friend class const_iterator;
        private:
            // Your private members go here
            BTree* Tree;
            PTR_FILE_CONT offset_leafnode;//记录所在叶子节点的块
            short pos_in_leafnode;//0-Based
        public:
            bool modify(const Value& value){
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                tmpl.leaf[pos_in_leafnode].valdata = value;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fwrite(&tmpl, sizeof(Node_leaf),1,Tree->ptr_file);
                return true;
            }
            Value getValue(){
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                return tmpl.leaf[pos_in_leafnode].valdata;
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
            iterator(const const_iterator& other){
                Tree = other.Tree;
                offset_leafnode = other.offset_leafnode;
                pos_in_leafnode = other.pos_in_leafnode;
            }
            // Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                PTR_FILE_CONT tmpoffset = offset_leafnode;
                short tmppos = pos_in_leafnode;
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                if(pos_in_leafnode == tmpl.curnum - 1){
                    if(tmpl.next == -1) {
                        *this = Tree->end();
                        return iterator(Tree,tmpoffset,tmppos);
                    }
                    //if(tmpl.next == -1) throw in_iterator();
                    offset_leafnode = tmpl.next;
                    pos_in_leafnode = 0;
                    return iterator(Tree,tmpoffset,tmppos);
                }
                ++pos_in_leafnode;
                return iterator(Tree,tmpoffset,tmppos);
            }
            iterator& operator++() {
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                if(pos_in_leafnode == tmpl.curnum - 1){
                    if(tmpl.next == -1){
                        *this = Tree->end();
                        return *this;
                    }
                    offset_leafnode = tmpl.next;
                    pos_in_leafnode = 0;
                    return *this;
                }
                ++pos_in_leafnode;
                return *this;
            }
            iterator operator--(int) {
                PTR_FILE_CONT tmpoffset = offset_leafnode;
                short tmppos = pos_in_leafnode;
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                if(pos_in_leafnode == 0){
                    if(tmpl.prev == -1) throw "in iterator --:the first leaf";
                    offset_leafnode = tmpl.prev;
                    fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                    fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                    pos_in_leafnode = tmpl.curnum - 1;
                    return iterator(Tree, tmpoffset,tmppos);
                }
                --pos_in_leafnode;
                return iterator(Tree, tmpoffset,tmppos);
            }
            iterator& operator--() {
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                if(pos_in_leafnode == 0){
                    if(tmpl.prev == -1) throw "in iterator --:the first leaf";
                    offset_leafnode = tmpl.prev;
                    fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                    fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                    pos_in_leafnode = tmpl.curnum - 1;
                    return *this;
                }
                --pos_in_leafnode;
                return *this;
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                if(rhs.Tree == Tree&&rhs.offset_leafnode == offset_leafnode
                   &&rhs.pos_in_leafnode == pos_in_leafnode)
                    return true;

                else return false;
            }
            bool operator==(const const_iterator& rhs) const {
                if(rhs.Tree == Tree&&rhs.offset_leafnode == offset_leafnode
                   &&rhs.pos_in_leafnode == pos_in_leafnode)
                    return true;
                else return false;
            }
            bool operator!=(const iterator& rhs) const {
                return !(*this == rhs);
            }
            bool operator!=(const const_iterator& rhs) const {
                return !(*this == rhs);
            }
        };
        class const_iterator {
            friend class iterator;
            friend class BTree;
            // it should has similar member method as iterator.
            //  and it should be able to construct from an iterator.
        private:
            // Your private members go here
            const BTree* Tree;
            PTR_FILE_CONT offset_leafnode;//记录所在叶子节点的块
            int pos_in_leafnode;
        public:
            Value getValue(){
                Node_leaf tmpl;
                fseek(Tree->ptr_file,offset_leafnode,SEEK_SET);
                fread(&tmpl,sizeof(Node_leaf),1,Tree->ptr_file);
                return tmpl.leaf[pos_in_leafnode].valdata;
            }
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
            const_iterator(const BTree* t,PTR_FILE_CONT o,short p){
                Tree = t;
                offset_leafnode = o;
                pos_in_leafnode = p;
            };
            // And other methods in iterator, please fill by yourself.
            bool operator==(const iterator& rhs) const {
                if(rhs.Tree == Tree&&rhs.offset_leafnode == offset_leafnode
                   &&rhs.pos_in_leafnode == pos_in_leafnode)
                    return true;
                else return false;
            }
            bool operator==(const const_iterator& rhs) const {
                if(rhs.Tree == Tree&&rhs.offset_leafnode == offset_leafnode
                   &&rhs.pos_in_leafnode == pos_in_leafnode)
                    return true;
                else return false;
            }
            bool operator!=(const iterator& rhs) const {
                return !(*this == rhs);
            }
            bool operator!=(const const_iterator& rhs) const {
                return !(*this == rhs);
            }
        };
        // Default Constructor and Copy Constructor
        BTree():ptr_file(nullptr),isbuildchain(false),
                tmpindex(nullptr),tmpleaf(nullptr),leaf(nullptr),indexstack(nullptr){
            BPlusTree = new BPT;
            strcpy(name,EMPTY_NAME_FILE);
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
            //若空，建树。
            if(empty()){
                new_tmpleaf();
                tmpleaf->leaf[0].keydata = key;
                tmpleaf->leaf[0].valdata = value;
                tmpleaf->curnum = 1;
                tmpleaf->myself = IO_alloc();
                IO_write(tmpleaf);

                BPlusTree->sumnum_data = 1;
                BPlusTree->height = 1;
                BPlusTree->root = BPlusTree->firstleaf = tmpleaf->myself;
                IO_write_nodeBPT();
                return pair<iterator, OperationResult>
                        (iterator(),Success);
            }
            //非空，第一步正常find并且插入叶子。
            INS_inleaf(key,value);
            //之后“递归”修改并且写回文件
            //若不需要分裂,写入叶节点和基本信息即可。
            if(leaf->curnum < MAX_NUM_LEAF){
                IO_write(leaf);
                IO_write_nodeBPT();
                destroy_chain();
                return pair<iterator, OperationResult>
                        (iterator(),Success);
            }

            data_index info_pushup; //该变量用来存储向上传递的信息。
            PTR_FILE_CONT son = leaf->myself;//用于查找父亲中自己的位置。
            //分裂叶节点
            INS_splitleaf(info_pushup);
            if(BPlusTree->height <= 1) {
                INS_resetroot(info_pushup);
                destroy_chain();
                return pair<iterator, OperationResult>
                        (iterator(),Success);
            }
            //向上插入
            Node_index tmpind = indexstack->pop();
            INS_inindex(info_pushup,tmpind,son);
            son = tmpind.myself;
            while(tmpind.curnum == MAX_NUM_INDEX - 1){
                //分裂索引节点
                INS_splitindex(info_pushup,tmpind);
                if(indexstack->isStackempty()){
                    INS_resetroot(info_pushup);
                    destroy_chain();
                    return pair<iterator, OperationResult>
                            (iterator(),Success);
                }
                //向上插入
                tmpind = indexstack->pop();
                INS_inindex(info_pushup,tmpind,son);
                son = tmpind.myself;
            }
            /*
            if(tmpind.curnum < MAX_NUM_INDEX - 1){
                IO_write(&tmpind);
                IO_write_nodeBPT();
            }else if(indexstack->isStackempty()){
                INS_resetroot(info_pushup);
            }*/
            /*
            if(indexstack->isStackempty()) INS_resetroot(info_pushup);
            else if(tmpind.curnum < MAX_NUM_INDEX - 1) {
                IO_write(&tmpind);
                IO_write_nodeBPT();
            }*/
            IO_write(&tmpind);
            IO_write_nodeBPT();
            destroy_chain();
            return pair<iterator, OperationResult>
                    (iterator(),Success);
        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        OperationResult erase(const Key& key) {
            // TODO erase function
            return Fail;  // If you can't finish erase part, just remaining here.
        }
        // Return a iterator to the beginning
        ///如果是空树的话到底返回什么呢？
        iterator begin() {
            if(empty()) throw "in begin():empty tree";
            return iterator(this,BPlusTree->firstleaf,0);
        }
        const_iterator cbegin() const {
            if(empty()) throw "in cbegin():empty tree";
            return const_iterator(this,BPlusTree->firstleaf,0);
        }
        // Return a iterator to the end(the next element after the last)
        ///如果是空树的话到底返回什么呢？
        iterator end() {
            if(empty()) throw "in end():empty tree";
            End_getleaf();
            return iterator(this,tmpleaf->myself,tmpleaf->curnum);
        }
        const_iterator cend() const {
            if(BPlusTree->sumnum_data == 0) throw "in cend():empty tree";
            Node_leaf tmpl;
            FILE* tmpptrf = ptr_file;
            fseek(tmpptrf,BPlusTree->firstleaf,SEEK_SET);
            fread(&tmpl, sizeof(Node_leaf),1,tmpptrf);

            while(tmpl.next != -1){
                fseek(tmpptrf,tmpl.next,SEEK_SET);
                fread(&tmpl, sizeof(Node_leaf),1,tmpptrf);
            }
            return const_iterator(this,tmpl.myself,tmpl.curnum);
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
            if(isbuildchain) destroy_chain();
            isbuildchain = false;
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
            iterator tmp = find(key);
            if(tmp == end()) throw "in at:key not exist";

            new_tmpleaf();
            IO_getleaf(tmp.offset_leafnode);
            return tmpleaf->leaf[tmp.pos_in_leafnode].valdata;
        }
        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key) const {
            const_iterator tmp = find(key);
            if(tmp == cend()) return 0;
            else return 1;
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
            if(empty()) throw "in it_find:try to find in an empty tree";
            short pos;
            Find_getleaf(key);
            pos = Find_seek_leaf(tmpleaf,key);
            if(pos != -1) return iterator(this,tmpleaf->myself,pos);
            else return end();
        }
        //呜呜呜我到这里才意识到自己在最初写的时候考虑是有多不周全……
        //把一些临时用的leaf，index放在这个类里面当作“全局变量”来使用= =
        //果然不听老师言吃亏在眼前。以后记住了，临时用的东西一定要在需要的函数内就地解决。
        const_iterator find(const Key& key) const {
            if(empty()) throw "in const_it_find:try to find in an empty tree";
            Node_leaf tmpl;
            Node_index tmpi;
            FILE* tmpptrf = ptr_file;
            PTR_FILE_CONT tmpnext = BPlusTree->root;
            int i = BPlusTree->height;

            if(BPlusTree->height > 1) {
                while (i > 1) {
                    fseek(tmpptrf, tmpnext, SEEK_SET);
                    fread(&tmpi, sizeof(Node_index), 1, tmpptrf);
                    tmpnext = Find_seek_index(&tmpi, key);
                    --i;
                }
            }
            fseek(tmpptrf,tmpnext,SEEK_SET);
            fread(&tmpl, sizeof(Node_leaf),1,tmpptrf);
            short pos;
            pos = Find_seek_leaf(&tmpl,key);
            if(pos != -1) return const_iterator(this,tmpl.myself,pos);
            else return cend();
        }
    };
}  // namespace sjtu