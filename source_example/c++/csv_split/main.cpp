#include <iostream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <unordered_map>

// 统计时间用
#define BEGIN_TIME(__begin) \
    auto __begin = std::chrono::high_resolution_clock::now()
#define END_TIME(__begin) \
    do { \
    auto __end = std::chrono::high_resolution_clock::now(); \
    auto __interval = std::chrono::duration_cast<std::chrono::milliseconds>(__end - __begin); \
    std::cout << #__begin << " use " << __interval.count() << "ms" << std::endl; \
    } while (false)

static const size_t MEM_NODE_SIZE = 1*1024*1024; // 写缓存大小
static const size_t READ_BUF_SIZE = 1*1024*1024; // 读缓存大小

// 缓存节点，单向链表
struct mem_node
{
    static const size_t _mem_size = MEM_NODE_SIZE;

    mem_node()
    {
        buf = new char[_mem_size];
        next = nullptr;
        used = 0;
    }

    ~mem_node()
    {
        delete[] buf;
    }

    inline size_t free() const { return _mem_size - used; }

    // 添加数据，返回已添加的数据长度
    inline size_t add(const char* data, size_t len)
    {
        if (len > free()) {
            // len - free() = 超出缓存的数据长度
            len = len - (len - free());
        }
        std::memcpy(&buf[used], data, len);
        used += len;
        return len;
    }

    char *buf;
    size_t used;
    mem_node *next;
};

// 缓存节点的链表头
// symbol->head->node->node ...
// 一个符号相关的数据缓存在该单向链表内
struct mem_node_head
{
    mem_node_head()
    {
        head = nullptr;
        last = nullptr;
    }

    ~mem_node_head()
    {
        while (head != nullptr) {
            mem_node *next = head->next;
            delete head;
            head = next;
        }
    }

    inline void add(const char* data, size_t size)
    {
        if (head == nullptr) {
            head = new mem_node();
            last = head;
        }
        size_t used = last->add(data, size);
        if (used != size) {
            mem_node* new_last = new mem_node();
            last->next = new_last;
            last = new_last;
            if (last->add(&data[used], (size-used)) != (size-used)) {
                std::cerr << "data too long : data_size=" << size
                          << " mem_node::size=" << mem_node::_mem_size
                          << std::endl;
                std::exit(-1);
            }
        }
    }

    size_t save(const std::string& path)
    {
        std::string save_name = path + this->name + ".csv";
        std::FILE *save_fp = std::fopen(save_name.c_str(), "ab+");
        if (save_fp == nullptr) {
            std::cerr << "save file failed : symbol=" << save_name << std::endl;
            std::exit(-1);
        }
        size_t mem_size = 0;
        // 写入表头
        std::fseek(save_fp, 0, SEEK_END);
        // if (std::feof(save_fp)) {
        if (0 == std::ftell(save_fp)) {
            const char* head = "Symbol,TradingDate,TradingTime,RecID,TradeChannel,TradePrice,TradeVolume,TradeAmount,UNIX,Market,BuyRecID,SellRecID,BuySellFlag,SecurityID\n"; // 注意换行符一定要有
            if (std::fwrite(head, 1, std::strlen(head), save_fp) != std::strlen(head)) {
            std::cerr << "write file failed : symbol=" << this->name
                << " err=" << std::strerror(std::ferror(save_fp))
                << std::endl;
            std::exit(-1);
            }
        }
        // 依次遍历内存数据，并写入文件内
        for (auto node = this->head; node != nullptr; node = node->next) {
            if (std::fwrite(node->buf, 1, node->used, save_fp) != node->used) {
                std::cerr << "write file failed : symbol=" << this->name
                          << " err=" << std::strerror(std::ferror(save_fp))
                          << std::endl;
                std::exit(-1);
            }
            mem_size += node->used;
        }
        std::fclose(save_fp);
        return mem_size;
    }

    mem_node *head;
    mem_node *last;

    std::string name;
};

// C语言字符串 HASH 函数
// 如果使用 std::string 转换代价太高
static inline unsigned long cstrhash(const char *str, size_t len)
{
    unsigned long hash = 5381;

    for (size_t i = 0; i < len; i++) {
        unsigned long c = static_cast<unsigned long>(str[i]);
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

typedef std::unordered_map<unsigned long, mem_node_head*> mem_node_map;
typedef std::pair<unsigned long, mem_node_head*> mem_node_pair;
static mem_node_map mem_nodes;

// 数据检查
static inline void line_check(const char* begin, size_t size, const size_t line_count)
{
    int dot_count = 0;
    for (size_t i = 0; i < size; i++) {
        if (begin[i] == ',') {
            dot_count++;
        }
    }
    if (dot_count != 13) {
        std::string line = std::string(begin, 0, size);
        std::cerr << "line_count=" << line_count
                  << " | dot_count=" << dot_count
                  << " | data=" << line << std::endl;
        std::exit(-1);
    }
}

static void parse_csv(const char* buf, size_t size, size_t& line_count)
{
    //BEGIN_TIME(__parse_csv);
    const char* sym = nullptr;
    const char* cur = buf;
    const char* beg = buf;
    const char* end = &buf[size];

    while (cur != end) {
        if (*cur == '\n') {
            if (sym == nullptr) {
                std::cerr << " invalid data sym is nil, data="
                          << std::string(beg, 0, size_t(cur-beg))
                          << " line_count=" << line_count
                          << std::endl;
                std::exit(-1);
            }

#if 1
            auto symbol = cstrhash(beg, size_t(sym-beg));
            mem_node_head* head = nullptr;

            auto it = mem_nodes.find(symbol);
            if (it == mem_nodes.end()) {
                head = new mem_node_head();
                mem_nodes.insert(mem_node_pair(symbol, head));
                head->name = std::string(beg, 0, size_t(sym-beg));
            } else {
                head = it->second;
            }
            head->add(beg, size_t(cur-beg)+1);
#else
            line_check(beg, size_t(cur-beg), line_count);
#endif

            sym = nullptr;
            beg = cur + 1;
            line_count++;
        } else if (sym == nullptr && *cur == ',') {
            sym = cur;
        }
        size--;
        cur++;
    }

    //END_TIME(__parse_csv);
}

static void load_csv(const char* filename)
{
    BEGIN_TIME(__load_csv);

    std::FILE* fp = std::fopen(filename, "rb");
    if (fp == nullptr) {
        std::cerr << "open file failed : " << filename << std::endl;
        std::exit(-1);
    }

    std::setvbuf(fp, nullptr, _IONBF, 0);
    std::fseek(fp, 0, SEEK_END);

    long all_read_bytes = 0;
    long file_size = std::ftell(fp);
    size_t line_count = 0;

    std::fseek(fp, 0, SEEK_SET);

#if 1
    char *buf = new char[READ_BUF_SIZE];
    size_t buf_used = 0;

    while (!std::feof(fp)) {
        // buf_used 表示未处理的数据，避免覆盖
        auto read_bytes = std::fread(&buf[buf_used], 1, READ_BUF_SIZE-buf_used, fp);
        if (read_bytes != (READ_BUF_SIZE-buf_used) && !std::feof(fp)) {
            std::cerr << "read failed : " << std::strerror(std::ferror(fp)) << std::endl;
            std::exit(-1);
        }
        all_read_bytes += read_bytes;
        // 缓存总数据 = 缓存之前的数据 + 当前读取到的数据
        auto data_size = buf_used + read_bytes;
        // 处理：挑取不完整的数据
        // 寻找最后一个换行符位置，换行符是完整数据条件
        size_t last_ln = data_size - 1;
        for (; last_ln > 0; last_ln--) {
            if (buf[last_ln] == '\n')
                break;
        }
        if (last_ln == 0) {
            std::cerr << "invalid data : line=" << line_count << " no ln" << std::endl;
            std::exit(-1);
        }
        // 处理数据：last_ln 保证了传递的数据都是完整的，因为其指向了最后一个换行符（下标）
        parse_csv(buf, last_ln+1, line_count);
        // 处理末尾不完整的数据
        // 把未处理的数据移动至缓存的头部，避免被覆盖
        if (last_ln < (data_size - 1)) {
            buf_used = data_size - 1 - last_ln;
            std::memcpy(buf, &buf[last_ln+1], buf_used);
        } else {
            buf_used = 0;
        }
    }
#else
    char *buf = new char[file_size];
    all_read_bytes = static_cast<long>(std::fread(buf, 1, static_cast<size_t>(file_size), fp));
    if (all_read_bytes != file_size) {
        std::cerr << "read failed : " << std::strerror(std::ferror(fp)) << std::endl;
        std::exit(-1);
    }
    parse_csv(buf, static_cast<size_t>(all_read_bytes), line_count);
#endif

    std::cout << "file_size=" << file_size
              << " all_read_bytes=" << all_read_bytes
              << " eq=" << (file_size == all_read_bytes ? "true" : "false")
              << std::endl;

    std::fclose(fp);
    delete[] buf;

    END_TIME(__load_csv);
}

int main(int argc, char **argv)
{
    std::cout << "argc=" << argc << " argv[0]=" << argv[0] << std::endl;
    load_csv("data.csv");

    std::cout << "mem_nodes.size=" << mem_nodes.size() << std::endl;
    size_t all_mem_size = 0;

    BEGIN_TIME(__save_symbols);
    for (auto it : mem_nodes) {
        all_mem_size += it.second->save("data/");
    }
    END_TIME(__save_symbols);

    std::cout << "all_mem_size=" << all_mem_size << " byte" << std::endl;
    std::cout << "all_mem_size=" << all_mem_size/(1024*1024) << " mb" << std::endl;
    return 0;
}
