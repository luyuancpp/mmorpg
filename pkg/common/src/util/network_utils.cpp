#include "network_utils.h"

#include <boost/asio.hpp>
#include <iostream>

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

void ParseIpPort(const std::string& input, std::string& ip, uint16_t& port) {
	// 将输入字符串解析成 IP 地址和端口
	std::istringstream ss(input);
	std::string host;

	// 使用 ':' 分隔 IP 和端口
	std::getline(ss, host, ':');
	ip = host;  // 提取 IP 地址部分

	// 获取端口
	std::getline(ss, host);
	port = std::stoi(host);  // 转换为整数端口
}


std::string FormatIpAndPort(const std::string& ip, uint32_t port)
{
	return ip + ":" + std::to_string(port);
}