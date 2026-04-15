/**
 * @file ipc_endpoint.h
 * @brief IPC通信端点抽象基类
 * @details 定义了跨进程通信端点的通用接口，包含数据发送、接收、接收回调等核心功能
 *          所有具体IPC通信实现类（如UDP、管道等）需继承此类并实现纯虚函数
 */

 #ifndef IPC_ENDPOINT_H
 #define IPC_ENDPOINT_H
 
 #include <vector>       // 用于存储接收/发送的字节数据
 #include <functional>   // 用于std::function，实现回调函数封装
 #include <cstdint>      // 用于标准整数类型（uint8_t）
 
 /**
  * @class IpcEndpoint
  * @brief IPC通信端点抽象基类
  * @note 抽象类，不能直接实例化，必须由子类实现send/recv纯虚函数
  */
 class IpcEndpoint {
 public:
     /**
      * @brief 定义数据接收回调函数类型别名
      * @param const std::vector<uint8_t>& 接收到的字节数据容器
      * @details 当接收到数据时，会通过该类型的回调函数将数据传递出去
      */
     using RecvCallback = std::function<void(const std::vector<uint8_t>&)>;
 
     /**
      * @brief 虚析构函数
      * @details 基类必须定义虚析构函数，确保子类对象销毁时能正确调用子类析构函数
      */
     virtual ~IpcEndpoint() = default;
 
     /**
      * @brief 纯虚函数：发送数据
      * @param data 待发送的数据缓冲区指针（uint8_t类型字节流）
      * @param len 待发送的数据长度（字节数）
      * @return int 发送成功返回发送的字节数，失败返回负数错误码
      */
     virtual int send(const uint8_t* data, size_t len) = 0;
 
     /**
      * @brief 纯虚函数：接收数据
      * @param buffer 接收数据的缓冲区指针
      * @param maxlen 缓冲区最大可接收的字节数
      * @return int 接收成功返回接收到的字节数，失败返回负数错误码
      */
     virtual int recv(uint8_t* buffer, size_t maxlen) = 0;
 
     /**
      * @brief 设置数据接收回调函数
      * @param cb 外部传入的回调函数对象
      * @details 子类接收到数据后，可通过调用recv_callback_将数据通知给外部
      */
     void setRecvCallback(RecvCallback cb) {
         // 移动语义赋值，减少拷贝开销
         recv_callback_ = std::move(cb);
     }
 
 protected:
     /**
      * @brief 接收回调函数对象
      * @details 保护成员，子类可直接访问，用于触发数据接收回调
      */
     RecvCallback recv_callback_;
 };
 
 #endif // IPC_ENDPOINT_H