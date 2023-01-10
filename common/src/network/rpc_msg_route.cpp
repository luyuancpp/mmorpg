#include "rpc_msg_route.h"

const int32_t kMaxRouteSize = 20;

thread_local ThreadLocalRoteMsgBodyList g_route_msg;

RouteMsgBody route2db;
RouteMsgBody route2controller;
RouteMsgBody route2gate;
std::string route_msg_body;