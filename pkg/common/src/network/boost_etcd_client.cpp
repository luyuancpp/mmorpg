#include "boost_etcd_client.h"
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>

namespace beast = boost::beast;      // 从 Boost.Beast 引入命名空间
namespace asio = boost::asio;        // 从 Boost.Asio 引入命名空间
namespace json = boost::json;        // 从 Boost.JSON 引入命名空间

// 用于处理响应的函数
void handle_response(beast::http::response<beast::http::string_body>& response) {
	std::cout << "Response: " << response.body() << std::endl;
}

// 创建一个 HTTP 客户端，向 etcd 发送请求
void send_request(const std::string& host, const std::string& port,
				  const std::string& target, const std::string& method,
				  const std::string& payload) {

	asio::io_context ioc;  // 创建 I/O 上下文
	beast::tcp_stream stream(ioc);  // 创建流

	// 解析服务器地址
	asio::ip::tcp::resolver resolver(ioc);
	auto const results = resolver.resolve(host, port);
	stream.connect(results);  // 连接到服务器

	// 创建 HTTP 请求
	beast::http::request<beast::http::string_body> req;
	req.version(11);
	req.method(method == "POST" ? beast::http::verb::post : beast::http::verb::get);
	req.target(target);
	req.set(beast::http::field::host, host);
	req.set(beast::http::field::content_type, "application/json");
	req.body() = payload;
	req.prepare_payload();

	// 发送请求
	beast::http::write(stream, req);

	// 接收响应
	beast::http::response<beast::http::string_body> res;
	beast::flat_buffer buffer;  // 创建缓冲区来接收响应

	// 使用 flat_buffer 读取响应
	beast::http::read(stream, buffer, res);

	// 处理响应
	handle_response(res);

	// 关闭连接
	beast::error_code ec;
	stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
}

std::string create_lease_request(int ttl) {
	json::object lease_obj;
	lease_obj["TTL"] = ttl;

	// 将 JSON 转换为字符串
	return json::serialize(lease_obj);
}

std::string register_service_request(const std::string& service_name,
	const std::string& service_address,
	const std::string& lease_id) {
	json::object register_obj;
	register_obj["key"] = "services/" + service_name;
	register_obj["value"] = service_address;
	register_obj["lease"] = lease_id;

	// 将 JSON 转换为字符串
	return json::serialize(register_obj);
}

std::string renew_lease_request(const std::string& lease_id) {
	json::object renew_obj;
	renew_obj["ID"] = lease_id;

	// 将 JSON 转换为字符串
	return json::serialize(renew_obj);
}
