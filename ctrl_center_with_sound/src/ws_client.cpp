#include "ws_client.h"
#include "json.hpp"
#include <iostream>
#include <thread>
#include <cstring>
#include <openssl/x509v3.h>

using json = nlohmann::json;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// ==================== 构造 / 析构 ====================
WebSocketClient::WebSocketClient(
    std::string host,
    std::string port,
    std::string path,
    std::string hello_msg,
    std::string headers
)
    : m_client(new Client())
    , m_host(std::move(host))
    , m_port(std::move(port))
    , m_path(std::move(path))
    , m_hello_msg(std::move(hello_msg))
    , m_headers(std::move(headers))
    , m_is_connected(false)
    , m_is_shaked(false)
    , m_running(true)
{}

WebSocketClient::~WebSocketClient() {
    m_running = false;
    if (m_client) {
        websocketpp::lib::error_code ec;
        m_client->close(m_hdl, websocketpp::close::status::going_away, "destruct", ec);
        m_client->stop();
        delete m_client;
        m_client = nullptr;
    }
}

// ==================== 回调设置 ====================
void WebSocketClient::set_callbacks(RecvCallback bin_cb, RecvCallback txt_cb, CloseCallback close_cb) {
    m_bin_cb = std::move(bin_cb);
    m_txt_cb = std::move(txt_cb);
    m_close_cb = std::move(close_cb);
}

// ==================== 发送接口 ====================
int WebSocketClient::send_binary(const char* data, int size) {
    if (!m_is_connected || !m_is_shaked || !data || size <= 0)
        return -1;

    try {
        m_client->send(m_hdl, data, size, websocketpp::frame::opcode::binary);
    } catch (...) {
        return -1;
    }
    return 0;
}

int WebSocketClient::send_text(const char* data, int size) {
    if (!m_is_connected || !data || size <= 0)
        return -1;

    try {
        m_client->send(m_hdl, data, size, websocketpp::frame::opcode::text);
        m_is_shaked = true;
    } catch (...) {
        return -1;
    }
    return 0;
}

// ==================== 状态查询 ====================
bool WebSocketClient::is_connected() const {
    return m_is_connected;
}

// ==================== 核心事件处理 ====================
void WebSocketClient::on_open(ConnectionHandle hdl) {
    m_hdl = hdl;
    m_is_connected = true;
    std::cout << "Connection opened" << std::endl;

    // 发送握手消息
    try {
        m_client->send(hdl, m_hello_msg, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        std::cerr << "Send hello failed: " << e.what() << std::endl;
    }
}

void WebSocketClient::on_close(ConnectionHandle hdl) {
    m_is_connected = false;
    m_is_shaked = false;

    auto con = m_client->get_con_from_hdl(hdl);
    std::cout << "Connection closed. Code: " << con->get_remote_close_code() 
              << ", Reason: " << con->get_remote_close_reason() << "!!" << std::endl;

    if (m_close_cb) {
        m_close_cb(con->get_remote_close_code());
    }
}

void WebSocketClient::on_message(ConnectionHandle hdl, Client::message_ptr msg) {
    auto op = msg->get_opcode();
    const auto& payload = msg->get_payload();

    if (op == websocketpp::frame::opcode::binary) {
        if (m_bin_cb)
            m_bin_cb(payload.data(), payload.size());
        return;
    }

    if (op == websocketpp::frame::opcode::text) {
        std::cout << "[Recv] " << payload << std::endl;
        try {
            if (m_txt_cb)
                m_txt_cb(payload.data(), payload.size());
        } catch (json::parse_error& e) {
            std::cout << "Failed to parse JSON message: " << e.what() << std::endl;
        } catch (std::exception& e) {
            std::cout << "Error processing message: " << e.what() << std::endl;
        }
    }
}

// ==================== TLS 证书验证 ====================
bool WebSocketClient::verify_subject_alternative_name(const char* hostname, X509* cert) {
    STACK_OF(GENERAL_NAME)* san = (STACK_OF(GENERAL_NAME)*)X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr);

    if (!san) return false;

    bool ok = false;
    int cnt = sk_GENERAL_NAME_num(san);
    for (int i = 0; i < cnt; ++i) {
        auto gn = sk_GENERAL_NAME_value(san, i);
        if (gn->type != GEN_DNS) continue;

        const char* dns = (const char*)ASN1_STRING_get0_data(gn->d.dNSName);
        if (strcasecmp(hostname, dns) == 0) {
            ok = true;
            break;
        }
    }
    sk_GENERAL_NAME_pop_free(san, GENERAL_NAME_free);
    return ok;
}

bool WebSocketClient::verify_common_name(const char* hostname, X509* cert) {
    int idx = X509_NAME_get_index_by_NID(X509_get_subject_name(cert), NID_commonName, -1);
    if (idx < 0) return false;

    auto entry = X509_NAME_get_entry(X509_get_subject_name(cert), idx);
    auto asn1 = X509_NAME_ENTRY_get_data(entry);
    const char* cn = (const char*)ASN1_STRING_get0_data(asn1);
    return strcasecmp(hostname, cn) == 0;
}

bool WebSocketClient::verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx) {
    int depth = X509_STORE_CTX_get_error_depth(ctx.native_handle());
    if (depth == 0 && preverified) {
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        return verify_subject_alternative_name(m_host.c_str(), cert)
            || verify_common_name(m_host.c_str(), cert);
    }
    return preverified;
}

WebSocketClient::ContextPtr WebSocketClient::on_tls_init(ConnectionHandle hdl) {
    ContextPtr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::no_sslv3);

        ctx->set_verify_callback(bind(&WebSocketClient::verify_certificate, this, _1, _2));
    } catch (const std::exception& e) {
        std::cerr << "TLS init error: " << e.what() << std::endl;
    }
    return ctx;
}

// ==================== 连接建立 ====================
int WebSocketClient::connect() {
    std::string uri = "wss://" + m_host + ":" + m_port + m_path;
    std::cout << "Connect to: " << uri << std::endl;

    try {
        m_client->clear_access_channels(websocketpp::log::alevel::all);
        m_client->set_access_channels(websocketpp::log::alevel::app);
        m_client->set_error_channels(websocketpp::log::elevel::all); // 修复：开启错误日志
        m_client->init_asio();

        // 绑定类成员函数
        m_client->set_open_handler(bind(&WebSocketClient::on_open, this, _1));
        m_client->set_close_handler(bind(&WebSocketClient::on_close, this, _1));
        m_client->set_message_handler(bind(&WebSocketClient::on_message, this, _1, _2));
        m_client->set_tls_init_handler(bind(&WebSocketClient::on_tls_init, this, _1));

        websocketpp::lib::error_code ec;
        auto con = m_client->get_connection(uri, ec);
        if (ec) {
            std::cerr << "Connect error: " << ec.message() << std::endl;
            return -1;
        }

        // 解析并设置自定义头
        try {
            json headers = json::parse(m_headers);
            for (auto& [k, v] : headers.items()) {
                con->append_header(k, v.get<std::string>());
            }
        } catch (...) {}

        m_client->connect(con);
    } catch (const std::exception& e) {
        std::cerr << "Connect exception: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}

// ==================== 线程运行 ====================
void WebSocketClient::run_thread() {
    try {
        connect();

        // 启动ASIO事件循环（阻塞运行）
        m_client->run();

        // run退出后，停止、释放资源
        m_client->stop();
        delete m_client;
        m_client = nullptr;

        std::cout << "exit from websocket_thread";
    } 
    catch (const websocketpp::exception& e) {
        std::cerr << "exit error! " << e.what();
    }
}

void WebSocketClient::start() {
    if (m_client == nullptr) {
        m_client = new Client();
    }
    std::thread(&WebSocketClient::run_thread, this).detach();
}