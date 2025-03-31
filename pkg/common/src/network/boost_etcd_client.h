#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

class AsyncHttpClient {
public:
	AsyncHttpClient(asio::io_context& io_context, const std::string& host, const std::string& port)
		: resolver_(io_context), socket_(io_context), host_(host), port_(port) {
	}

	// 异步发送 HTTP 请求
	void async_send_request() {
		resolver_.async_resolve(host_, port_,
			[this](boost::system::error_code ec, tcp::resolver::results_type results) {
				if (ec) {
					std::cerr << "Error resolving host: " << ec.message() << std::endl;
					return;
				}
				// 建立连接
				asio::async_connect(socket_, results,
					[this](boost::system::error_code ec, tcp::endpoint) {
						if (ec) {
							std::cerr << "Error connecting to server: " << ec.message() << std::endl;
							return;
						}
						// 连接成功后，发送 HTTP 请求
						send_http_request();
					});
			});
	}

private:
	// 发送 HTTP 请求
	void send_http_request() {
		// 构造 HTTP 请求
		http::request<http::string_body> req{ http::verb::get, "/v3/kv/put", 11 };
		req.set(http::field::host, host_);
		req.set(http::field::content_type, "application/json");
		req.body() = "{\"key\":\"my_key\",\"value\":\"my_value\"}";
		req.prepare_payload();

		// 异步发送请求
		http::async_write(socket_, req,
			[this](boost::system::error_code ec, std::size_t bytes_transferred) {
				if (ec) {
					std::cerr << "Error sending request: " << ec.message() << std::endl;
					return;
				}
				std::cout << "Sent " << bytes_transferred << " bytes" << std::endl;
				// 接收响应
				receive_http_response();
			});
	}

	// 接收 HTTP 响应
	void receive_http_response() {
		// 创建响应对象
		http::response<http::dynamic_body> res;

		// 异步读取响应
		http::async_read(socket_, buffer_, res,
			[this, res = std::move(res)](boost::system::error_code ec, std::size_t bytes_transferred) mutable {
				if (ec) {
					std::cerr << "Error reading response: " << ec.message() << std::endl;
					return;
				}
				std::cout << "Received " << bytes_transferred << " bytes" << std::endl;
				// 打印响应体
				std::cout << "Response: " << beast::buffers_to_string(res.body().data()) << std::endl;

				// 关闭连接
				socket_.shutdown(tcp::socket::shutdown_both, ec);
				if (ec) {
					std::cerr << "Error shutting down socket: " << ec.message() << std::endl;
				}
			});
	}

	asio::ip::tcp::resolver resolver_;
	asio::ip::tcp::socket socket_;
	asio::streambuf buffer_;
	std::string host_;
	std::string port_;
};
