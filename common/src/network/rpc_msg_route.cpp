#include "rpc_msg_route.h"

thread_local RouteMsgBody g_route2db_msg;
thread_local RouteMsgBody g_route2controller_msg;
thread_local RouteMsgBody g_route2gate_msg;