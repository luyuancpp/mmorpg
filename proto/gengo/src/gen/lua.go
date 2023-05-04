package gen

import (
	"gengo/config"
	"gengo/util"
	"os"
)

func LoadClientLua() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()
		var fds []os.DirEntry
		var err error
		if fds, err = os.ReadDir(config.ClientLuaDir); err != nil {
			return
		}
		data := "#include <sol/sol.hpp>\n" +
			"#include \"muduo/base/Logging.h\"\n" +
			"#include \"src/util/file2string.h\"\n" +
			"#include \"src/game_logic/thread_local/thread_local_storage_lua.h\"\n" +
			"void InitServiceLua()\n{\n"
		data += config.Tab + "std::string contents;\n"
		for _, fd := range fds {
			if !util.IsLuaFile(fd) {
				continue
			}
			data += config.Tab + "contents = common::File2String(\"" + config.ClientLuaProjectRelative + fd.Name() + "\");\n"
			data += config.Tab + "{\n"
			data += config.Tab2 + "auto r = tls_lua_state.script(contents);\n"
			data += config.Tab2 + "if (!r.valid())\n"
			data += config.Tab2 + "{\n"
			data += config.Tab3 + "sol::error err = r;\n"
			data += config.Tab3 + "LOG_FATAL << err.what();\n"
			data += config.Tab2 + "}\n"
			data += config.Tab + "}\n"
		}
		data += "\n}\n"
		Md5WriteData2File(config.ClientLuaServiceFile, data)
	}()

}
