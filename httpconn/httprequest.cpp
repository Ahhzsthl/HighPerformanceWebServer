#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;  //默认状态
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const{
    auto it = header_.find("Connection");
    if(it != header_.end()) {
        return it->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    auto it = post_.find(key);
    if(it != post_.end()) {
        return it->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    auto it = post_.find(key);
    if(it != post_.end()) {
        return it->second;
    }
    return "";
}

int HttpRequest::ConverHex(char ch) {
    //这转换时什么意思？ 
    //十六进制数，转换为十进制，只有ABCDEF
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch - '0';
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";     //http协议请求的起始行和请求头都以\r\n结尾
    if(buff.ReadableBytes() <= 0) {
        return false;
    }

    while(buff.ReadableBytes() && state_ != FINISH) {
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);     //每次读取一行进行解析
        switch(state_) {
            case REQUEST_LINE: 
                if(!ParseRequestLine_(line)) {
                    return false;
                }
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if(buff.ReadableBytes() <= 2) {
                    state_ = FINISH;
                    //缓冲区没有可读数据时，解析完毕
                }
                break;
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if(lineEnd == buff.BeginWriteConst()) {
            break;
            //缓冲区没有可读数据时，解析完毕
        }
        buff.RetrieveUntil(lineEnd + 2);    //更新缓冲区内读指针的位置，到下一行的起始处，以便循环读取下一行
    }
    return true;
}

bool HttpRequest::ParseRequestLine_(const std::string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    //http请求行例子： GET /index.html HTTP/1.1
    //起始符 匹配空格之外的字符任意次 空格 匹配空格之外的字符任意次 空格 HTTP/ 匹配空格之外的字符任意次 结束符
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        //匹配整个字符串 将匹配到的字符组存储到subMatch对象中
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;   //请求行只有一行，匹配完之后转换为匹配请求头的状态
        return true;
    }
    return false;
}

void HttpRequest::ParseHeader_(const std::string& line) {
    regex patten("^([^:]*): ?(.*)$");
    //起始符 匹配:之外的字符任意次 匹配: 匹配空格一次或零次 匹配任意字符任意次 结束符
    //[^]匹配不在括号内的字符 .表示匹配任意一个字符 ?匹配零次或一次 
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    } else {
        state_ = BODY;  //匹配不到说明要进行状态转换，匹配请求体
    }
    
}

void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
}

void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html";
    } else {
        for(auto& it : DEFAULT_HTML) {
            if(it == path_) {
                path_ += ".html";       //为什么是加.html   传进来的path_和DEFAULT_HTML对应，没有后缀
                break;
            }
        }
    }
}

void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-type"] == "application/x-www-form-urlencoded") {
        //url的编码方式，是html表单默认的编码类型
        //其他还有json，form-data，xml，二进制数据等类型
        ParseFromUrlencoded_();
        auto it = DEFAULT_HTML_TAG.find(path_);
        if(it != DEFAULT_HTML_TAG.end()) {
            int tag = it->second;
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } else {
                    path_ = "error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0)
        return;

    string key, value;
    int num = 0, n = body_.size(), i = 0, j = 0;
    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            //更新key
            break;
        case '+':
            //url编码方式，+表示空格
            body_[i] = ' ';
            break;
        case '%':
            //url编码方式，%表示遇到了两个十六进制数字，这是用十六进制表示的一个字符的ASCII码
            //这解析是什么意思？
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 1] = num % 10 + '0';
            body_[i + 2] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            //不同的键值对用&分割
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            //更新post_存储的请求数据对
            break;
        default:
            break;
        }
    }   
    assert(j <= i);
    //更新最后一个数据对
    auto it = post_.find(key);
    if(it == post_.end() && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin) {
    if(name == "" || pwd == "")
        return false;
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool isRegister = false;
    bool verify = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;

    if(!isLogin) {
        isRegister = true;  //不是登录，就是注册行为
    }
    //这段代码什么意思, sql中LIMIT什么意思，c_str函数什么意思
    //snprintf 将格式化的数据写入字符串，写入最大字符数为256
    //LIMIT 1 限制返回的行数，确保只返回一行数据，有重复数据时不用扫描整个表,查到第一行符合要求的数据后就返回
    snprintf(order, 256, "SELECT username, password FROM user WHERE username = '%s' LIMIT 1", name.c_str());

    //查询成功返回的是0，失败返回的是非0，所以要结束
    if(mysql_query(sql, order)) {   //执行sql查询
        mysql_free_result(res);     
        return false;
    }

    res = mysql_store_result(sql);      //检索查询的完整结果集
    j = mysql_num_fields(res);          //返回结果集中的字段数量
    fields = mysql_fetch_field(res);    //返回字段结构

    while(MYSQL_ROW row = mysql_fetch_row(res)) {       //从结果集中获取下一行,说明查到结果不为空，是登录
        string password(row[1]);
        //是登录行为
        if(isLogin) {
            if(pwd == password) {
                verify = true;
            } else {
                verify = false;
            }
        } else {
            verify = false;
        }
    }
    mysql_free_result(res);             //释放结果集占用的内存

    //注册行为 且用户名未被使用
    if(!isLogin && isRegister == true) {
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        if(mysql_query(sql, order)) {
            verify = false;
        }
        verify = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    return verify;
}

