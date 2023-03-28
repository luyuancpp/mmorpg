#include "gs_node.h"

GsNode::GsNode(const muduo::net::TcpConnectionPtr& conn)
	: session_(conn) 
{
	
}

GsNode::~GsNode()
{
	
}
