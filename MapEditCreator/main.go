package main

import (
	"embed"
	_ "embed"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
)

//go:embed all:frontend/dist
var assets embed.FS

//go:embed help.md
var helpMarkdown string

func main() {
	app := NewApp()

	err := wails.Run(&options.App{
		Title:     "美食大战老鼠重生-实验室地图插件编辑工具V2.4.9",
		Width:     1200,
		Height:    800,
		MinWidth:  820,
		MinHeight: 560,
		Frameless: true,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		BackgroundColour: &options.RGBA{R: 27, G: 38, B: 54, A: 1},
		OnStartup:        app.startup,
		OnBeforeClose:    app.beforeClose,
		Bind: []any{
			app,
		},
		DragAndDrop: &options.DragAndDrop{
			EnableFileDrop: true,
		},
	})

	if err != nil {
		println("Error:", err.Error())
	}
}
