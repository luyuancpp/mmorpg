﻿#include "network_utils.h"

#include <boost/asio.hpp>
#include <iostream>

std::string get_local_ip() {
	try {
		boost::asio::io_context io_context;  // 使用 io_context 替代 io_service

		// 这里是解析 TCP 地址，boost::asio::ip::tcp::resolver 解析的是域名或IP地址
		boost::asio::ip::tcp::resolver resolver(io_context);

		// 使用UDP解析器查询特定的DNS服务器，比如Google的DNS 8.8.8.8
		boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve("8.8.8.8", "80");

		// 返回第一个解析的 IP 地址
		for (auto& endpoint : endpoints) {
			return endpoint.endpoint().address().to_string();
		}

		return "127.0.0.1"; // 默认回退地址
	}
	catch (const std::exception& e) {
		std::cerr << "Error while retrieving local IP address: " << e.what() << std::endl;
		return "127.0.0.1"; // 默认回退地址
	}
}

uint16_t get_available_port(uint16_t start_port, uint16_t max_port) {
	boost::asio::io_context io_context;  // 使用 io_context 替代 io_service
	boost::asio::ip::tcp::acceptor acceptor(io_context);

	for (int port = start_port; port <= max_port; ++port) {
		try {
			boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
			acceptor.open(endpoint.protocol());
			acceptor.bind(endpoint);
			acceptor.listen();

			acceptor.close();
			return port;
		}
		catch (const boost::system::system_error& e) {
			std::cerr << "Error: " << e.what() << std::endl;  // 使用 e 来打印异常的描述信息
			continue;
		}
	}

	std::cerr << "No available port found in the given range!" << std::endl;
	return -1;  // 返回 -1 表示没有找到可用端口
}

