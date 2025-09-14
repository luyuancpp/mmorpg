syntax = "proto3";

option go_package = "game/generated/pb/game";

import "proto/db/mysql_database_table.proto";

message PlayerAllData {
  player_database player_database_data = 2;
  player_database_1 player_database_1_data = 3;
}
