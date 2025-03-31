#pragma once

#include <string>

// 函数声明
void send_request(const std::string& host, const std::string& port,
	const std::string& target, const std::string& method,
	const std::string& payload);

std::string create_lease_request(int ttl);

std::string register_service_request(const std::string& service_name,
	const std::string& service_address,
	const std::string& lease_id);

std::string renew_lease_request(const std::string& lease_id);

