#include "notice_struct.h"

bool operator!=(common::EnumCode l, const ReturnValue& r) { return r.num() != ReturnValue::ErrorNum(l); }