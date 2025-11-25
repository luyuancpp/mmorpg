package utils

import (
	"io"
	"io/fs"
	"os"
	"path/filepath"
)

// CopyFS 将fs.FS中的所有内容拷贝到目标目录
// fsys: 源文件系统（可以是os.DirFS、embed.FS等）
// targetDir: 目标目录（本地文件系统路径）
// 特点：递归拷贝所有文件和目录，覆盖已存在文件
func CopyFS(fsys fs.FS, targetDir string) error {
	// 遍历源文件系统中的所有内容
	return fs.WalkDir(fsys, ".", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		// 构建目标路径
		targetPath := filepath.Join(targetDir, path)

		if d.IsDir() {
			// 创建目标目录（包含权限）
			info, err := d.Info()
			if err != nil {
				return err
			}
			return os.MkdirAll(targetPath, info.Mode()&0777)
		}

		// 拷贝文件内容
		return copyFSFile(fsys, path, targetPath)
	})
}

// copyFSFile 拷贝fs.FS中的单个文件到本地文件系统
func copyFSFile(fsys fs.FS, srcPath, targetPath string) error {
	// 打开源文件（来自fs.FS）
	srcFile, err := fsys.Open(srcPath)
	if err != nil {
		return err
	}
	defer srcFile.Close()

	// 获取源文件信息（用于权限设置）
	srcInfo, err := srcFile.Stat()
	if err != nil {
		return err
	}

	// 创建目标文件（覆盖已存在文件）
	destFile, err := os.OpenFile(targetPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, srcInfo.Mode()&0777)
	if err != nil {
		return err
	}
	defer destFile.Close()

	// 拷贝文件内容
	_, err = io.Copy(destFile, srcFile)
	return err
}

// 便捷函数：拷贝本地目录（基于os.DirFS）
func CopyLocalDir(srcDir, targetDir string) error {
	// 将本地目录包装为fs.FS接口
	fsys := os.DirFS(srcDir)
	return CopyFS(fsys, targetDir)
}
