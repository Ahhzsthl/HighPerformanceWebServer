#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <string>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap
#include "../buffer/buffer.h"

class HttpResponse {
public : 
    HttpResponse();
    ~HttpResponse();
    void Init(const std::string& srcdir, const std::string& path, bool isKeepAlive = false, int code = -1);
    //带参数的初始化
    void MakeReponse(Buffer& buff);     //做出响应，响应的文件放在buff中，这是对外的接口
    void UnmapFile();                   //对文件取消映射
    char* File();                       //返回文件的指针
    size_t FileLen() const;             //返回文件的长度
    void ErrorContent(Buffer& buff, std::string message);   //添加错误信息
    int Code() {return code_;};

private : 
    int code_;                  //状态码
    bool keepAlive_;            //是否保持连接
    char* mmFile_;              //映射的文件指针
    struct stat mmFileStat_;    //文件的状态信息，在不同阶段会更新
    std::string srcDir_;        //文件夹路径
    std::string path_;          //文件路径

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;      //MIME类型,表示文档，文件，文件流的格式
    static const std::unordered_map<int, std::string> CODE_PATH;                //状态码对应的文档路径
    static const std::unordered_map<int, std::string> CODE_STATUS;              //状态码

    void ErrorHtml_();                  //判断是否是错误页面
    void AddStateLine_(Buffer &buff);   //添加状态行信息
    void AddHeader_(Buffer &buff);      //添加状态头信息
    void AddContent_(Buffer &buff);     //添加内容
    std::string GetFileType_();
};

#endif