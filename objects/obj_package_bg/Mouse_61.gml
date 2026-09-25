var _rows = package_tab_rows(package_button_select);   // 本页行数（自动扩容）
if package_button_select == 1{
	if y_offset < (_rows-8)*96 -40{
		y_offset += 40
	}
	else{
		y_offset = (_rows-8)*96
	}
}
else{
	if y_offset < (_rows-9)*88 -40{
		y_offset += 40
	}
	else{
		y_offset = (_rows-9)*88
	}
}
