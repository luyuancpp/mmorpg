package prototools

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestCppBuildRegistrationAddsNewDomainsWithoutDuplicates(t *testing.T) {
	for _, name := range []string{"CMakeLists.txt", "proto.vcxproj", "proto.vcxproj.filters"} {
		t.Run(name, func(t *testing.T) {
			text := "<Project>\r\n  <ItemGroup><ClCompile Include=\"chat\\chat.pb.cc\" /></ItemGroup>\r\n</Project>\r\n"
			if name == "CMakeLists.txt" {
				text = "# 保留工程配置\r\nset(SOURCE_FILES\r\n    chat/chat.pb.cc\r\n)\r\nadd_library(proto STATIC ${SOURCE_FILES})\r\n"
			}
			sources := []string{"chat/chat.pb.cc", "mail/mail.pb.cc", "rank/rank.pb.cc", "trade/commerce.pb.cc"}
			headers := []string{"mail/mail.pb.h"}
			first, err := appendCppProjectEntries(name, text, sources, headers)
			if err != nil {
				t.Fatal(err)
			}
			if !strings.Contains(strings.ReplaceAll(first, "\\", "/"), "mail/mail.pb.cc") {
				t.Fatal("新邮件协议未登记")
			}
			second, err := appendCppProjectEntries(name, first, sources, headers)
			if err != nil {
				t.Fatal(err)
			}
			if second != first {
				t.Fatal("重复运行改变清单或生成重复编译条目")
			}
			if !strings.HasPrefix(first, strings.Split(text, "\r\n")[0]) {
				t.Fatal("破坏已有配置")
			}
		})
	}
}

func TestCppBuildRegistrationRejectsBrokenManifestBeforeWriting(t *testing.T) {
	root := t.TempDir()
	protoRoot := filepath.Join(root, "proto")
	for _, project := range []string{"proto", "grpc_client"} {
		dir := filepath.Join(root, "cpp", "generated", project)
		if err := os.MkdirAll(dir, 0755); err != nil {
			t.Fatal(err)
		}
		for name, body := range map[string]string{"CMakeLists.txt": "set(SOURCE_FILES\n)\n", project + ".vcxproj": "<Project></Project>", project + ".vcxproj.filters": "<Project></Project>"} {
			if err := os.WriteFile(filepath.Join(dir, name), []byte(body), 0644); err != nil {
				t.Fatal(err)
			}
		}
	}
	if err := os.MkdirAll(filepath.Join(protoRoot, "mail"), 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(protoRoot, "mail", "mail.proto"), nil, 0644); err != nil {
		t.Fatal(err)
	}
	output := filepath.Join(root, "cpp", "generated", "proto", "mail")
	if err := os.MkdirAll(output, 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(output, "mail.pb.cc"), nil, 0644); err != nil {
		t.Fatal(err)
	}
	broken := filepath.Join(root, "cpp", "generated", "grpc_client", "grpc_client.vcxproj")
	if err := os.WriteFile(broken, []byte("<Project>"), 0644); err != nil {
		t.Fatal(err)
	}
	if err := syncCppBuildProjects(protoRoot, root); err == nil {
		t.Fatal("损坏清单必须失败")
	}
	unchanged, err := os.ReadFile(filepath.Join(root, "cpp", "generated", "proto", "proto.vcxproj"))
	if err != nil {
		t.Fatal(err)
	}
	if strings.Contains(string(unchanged), "mail.pb.cc") {
		t.Fatal("清单校验失败却写入半套登记")
	}
}
