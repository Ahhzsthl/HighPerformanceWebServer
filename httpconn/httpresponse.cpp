#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    //const定义的映射 代表着CODE_PATH不能被修改
    //那就禁止了CODE_PATH[code_]的这种访问形式，因为如果code_不在CODE_PATH中的话，就会创建一个新的映射，值为0
    //const定义的unordered_mao映射应该用at 或者find实现
    //CODE_PATH.at(code_)   //code_不存在时，at会抛出异常
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    keepAlive_ = false;
    srcDir_ = "";
    path_ = "";
    mmFile_ = nullptr;
    mmFileStat_ = {0};      //这样初始化时是什么意思？可以做别的初始化吗？
    //将结构体内的所有变量全都初始化为0
}

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const std::string& srcDir, const std::string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if(mmFile_) {
        UnmapFile();
    }
    code_ = code;
    keepAlive_ = isKeepAlive;
    srcDir_ = srcDir;
    path_ = path;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::MakeReponse(Buffer& buff) {
    //?为什么(srcDir_ + path_).data()是const char*类型, string也是stl库
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;    //文件不存在，或者文件是文件夹类型
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;    //文件不可读
    }
    else if(code_ == -1) {
        code_ = 200;    //初始化的状态
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message) {
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    auto it = CODE_STATUS.find(code_);
    if(it != CODE_STATUS.end()) {
        status = it->second;
    } else {
        status = "Bad Requese";
    }

    body += to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "<p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length" + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::ErrorHtml_() {
    auto it = CODE_PATH.find(code_);
    if(it != CODE_PATH.end()) {
        path_ = it->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);   //更新映射文件的状态信息
    }
}

void HttpResponse::AddStateLine_(Buffer &buff) {
    string status;
    auto it = CODE_STATUS.find(code_);      //第四个bug，CODE_STATUS写成了CODE_PATH，修改后有样式,
    //虽然不应现传输数据，但是会影响浏览器端的显示
    if(it != CODE_STATUS.end()) {
        status = it->second;
    } else {
        //防御性编程：如果code是一个未定义的值，就定义为默认的400错误码
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer &buff) {
    buff.Append("Connection: ");
    if(keepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer &buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) {
        ErrorContent(buff, "File NotFound");
        return;
    }

    //mmap的第一个参数为0代表什么意思 ： 第一个参数为0或NULL表示让内核选择映射的地址
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

std::string HttpResponse::GetFileType_() {
    //? string::size_type和npos是什么？什么情况下使用
    //size_type 是size_t类型，表示字符串的大小或索引
    //npos表示未找到或者无效的索引
    string::size_type idx = path_.find_last_of('.');    //找路径中最后一个.的位置，例子*****/test.html，找到的是.html
    if(idx == string::npos) {
        return "text/plain";    //未找到，返回默认的MIME类型
    }
    string suffix = path_.substr(idx);
    auto it = SUFFIX_TYPE.find(suffix);
    if(it != SUFFIX_TYPE.end()) {
        return it->second;
    }
    return "text/plain";
}