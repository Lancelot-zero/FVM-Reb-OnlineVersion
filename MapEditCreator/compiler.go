package main

import (
	"bytes"
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
	"unicode/utf8"

	"golang.org/x/text/encoding/simplifiedchinese"
)

const compilerName = "LabMapCompiler.exe"

// locateCompiler 查找编译器：
// 1. exe 同目录（发布形态：两个 exe 放在同一文件夹）
// 2. 当前工作目录（便于 wails dev：此时 exe 在临时构建目录里）
//
// 编译器不再内嵌。内嵌会导致运行时"自身释放可执行文件到 AppData 再执行"，
// 这正是杀毒软件启发式引擎判定 dropper 的最强特征，误报率极高。
func (a *App) locateCompiler() (string, error) {
	var tried []string

	if exePath, err := os.Executable(); err == nil {
		dir := filepath.Dir(exePath)
		side := filepath.Join(dir, compilerName)
		if _, err := os.Stat(side); err == nil {
			return side, nil
		}
		tried = append(tried, dir)
	}

	if wd, err := os.Getwd(); err == nil {
		side := filepath.Join(wd, compilerName)
		if _, err := os.Stat(side); err == nil {
			return side, nil
		}
		tried = append(tried, wd)
	}

	return "", fmt.Errorf("未找到 %s，请确认它与本程序放在同一文件夹内。已查找：%s",
		compilerName, strings.Join(tried, " 、 "))
}

// binPathFor 脚本 xxx.txt 对应输出 xxx.bin（同目录同名）
func binPathFor(scriptPath string) string {
	ext := filepath.Ext(scriptPath)
	return strings.TrimSuffix(scriptPath, ext) + ".bin"
}

// runCompiler 执行编译器，返回合并输出与退出码
func runCompiler(compilerPath, scriptPath string) (string, int, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, compilerPath, scriptPath)
	// 工作目录设为脚本所在目录，相对输出落在脚本旁
	cmd.Dir = filepath.Dir(scriptPath)
	// 隐藏控制台窗口
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: 0x08000000}

	var buf bytes.Buffer
	cmd.Stdout = &buf
	cmd.Stderr = &buf

	err := cmd.Run()
	exitCode := 0
	if err != nil {
		if ctx.Err() == context.DeadlineExceeded {
			return buf.String(), -1, fmt.Errorf("编译超时（60s）")
		}
		if ee, ok := err.(*exec.ExitError); ok {
			exitCode = ee.ExitCode()
		} else {
			return buf.String(), -1, fmt.Errorf("无法启动编译器: %w", err)
		}
	}
	return decodeText(buf.Bytes()), exitCode, nil
}

// decodeText 输出/文件内容解码：UTF-8 优先，失败回退 GBK（去 BOM）
func decodeText(data []byte) string {
	data = bytes.TrimPrefix(data, []byte{0xEF, 0xBB, 0xBF}) // UTF-8 BOM
	if utf8.Valid(data) {
		return string(data)
	}
	if s, err := simplifiedchinese.GBK.NewDecoder().Bytes(data); err == nil {
		return string(s)
	}
	return string(data)
}

// explorerExe 定位 explorer.exe（PATH 里可能没有，用 SystemRoot/常见路径兜底）
func explorerExe() string {
	if root := os.Getenv("SystemRoot"); root != "" {
		cand := filepath.Join(root, "explorer.exe")
		if _, err := os.Stat(cand); err == nil {
			return cand
		}
	}
	for _, p := range []string{`C:\Windows\explorer.exe`, `C:\WinNT\explorer.exe`} {
		if _, err := os.Stat(p); err == nil {
			return p
		}
	}
	return "explorer.exe" // 最后尝试 PATH 查找
}

// openInExplorer 在资源管理器中选中文件
func openInExplorer(path string) error {
	path = filepath.Clean(path)
	if fi, err := os.Stat(path); err != nil {
		return fmt.Errorf("文件不存在: %s", path)
	} else if fi.IsDir() {
		return exec.Command(explorerExe(), path).Start()
	}
	return exec.Command(explorerExe(), "/select,"+path).Start()
}
