function sh_disconnect(){
   if (global.network.mode == "client") {
	   disconnect();
    }else
	if (global.network.mode == "server"){
		closeserver();
	}else{
		return "当前并无连接"
	}
}
