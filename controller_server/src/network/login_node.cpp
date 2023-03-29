#include "login_node.h"

LoginNode::LoginNode(const muduo::net::TcpConnectionPtr& conn)
	: session_(conn) 
{
	
}

LoginNode::~LoginNode()
{
	
}
