function s(item, f, s) {
	var i;
	for(i=0; i<s.length; i++){
		if(item[f] === s[i][0]) {
			item[f] = s[i][1];
			return;
		}
	}
}
function rpc(s, m, d, f, n) {
	if (n) $("#ajaxnotification").show();

	$.getJSON("/rpc.cgi", 
		encodeURI(JSON.stringify({service: s, method: m, params: d, id: 0})),
		function(data) {
			if (n) $("#ajaxnotification").hide();
			if (data.error === null) f(data.result);
	});
}

function deal_info(item)
{
	s(item, "type", [[1,"dir"], [0, "file"]]);
	s(item, "status", [ [0, "local1"], [1, "local2"], [2, "remote"] ]);

	var unit;
	if (item.size > 1000000000) {
		unit = "GB";
		item.size = item.size / 1000000000;
	} else if (item.size > 1000000) {
		unit = "MB";
		item.size = item.size / 1000000;
	} else if (item.size > 1000) {
		unit = "KB";
		item.size = item.size / 1000;
	} else {
		unit = "B";
	}
	item.size = item.size.toFixed(1) + unit;

	if (item.status === "local1") {
		item.speed = "---";
		item.time = "已完成";
		item.progress = 100;
	} else if (item.status === "local2") {
		item.speed = "未知";
		item.time = "未知";
	} else if (item.status === "remote") {
		item.speed = "---";
		item.time = "一千年";
		item.progress = "0";
	}

}
LocalFile = {
	init: function() {
		this.load(".");
	},
	load: function(p) {
		rpc("systeminfo", "list_file", {path:p}, function(data){
			var i;
			for (i=0; i<data.length; i++) {
				if (data[i].type == 1)
					data[i].type = "i_file";
				if (data[i].type == 2)
					data[i].type = "i_dir";
				if (data[i].type == 3)
					data[i].type = "i_parent";
				if (data[i].type == 4)
					data[i].type = "i_driver";
			}
			$("#c_local ul.list").html($("#local_temp").render(data));
			LocalFile.do_bind();
			LocalFile.do_hover();
		});
	},
	do_bind: function() {
		$(".node").bind("click", function(n) {
			var type = $(this).find("div:first");
			if (type.hasClass("i_file"))
				FileList.add_file($(this).find("span").text());
			else
				LocalFile.load($(this).find("span").text());

		});
	},
	do_hover: function() {
		$(".node").hover(
		function() {
			$("#local_info").text("完整路径: "+$(this).find(".n_path").text());
		},
		function() {
			$("#local_info").text("双击图标就可以共享到局域网中了.");
		}
		);
	}
};

FileList = {
	init: function() {
		this.refresh_list();
	},
	refresh_list: function() {
		rpc("filemanager", "file_list", null, function(data){
			data = data || [];
			$("#c_file_list ul.list").html("");
			var i;
			for (i=0; i<data.length; i++) {
				FileList.add_info(data[i]);
			}
		});
	},
	add_info: function(node) {
		deal_info(node);
		$("#c_file_list ul.list").append($("#list_temp").render(node));

		install_contextmenu();

		$(".item").on("dblclick",function(){
			rpc("filemanager", "chunk_info", {hash:node.hash}, function(data){
				show_msg(data, "alert", false);
			});
		});
	},
	add_file: function(path) {
		rpc("filemanager", "add_file", {"path":path}, function(node) {
			show_msg(path+"  " +node);
		}, true);
	}
};

Updater = {
	update: function() {
		rpc("filemanager", "refresh", {}, function(data){
			files = data.newfile || [];
			payloads = data.pfile || [];
			progress = data.progress || [];
			var i;
			for(i=0; i<files.length; i++) {
				Updater.on_new_file(files[i]);
			}
			for(i=0; i<payloads.length; i++) {
				Updater.on_update_payload(payloads[i]);
			}
			for(i=0; i<progress.length; i++){
				Updater.on_update_progress(progress[i]);
			}
			var payload = data.payload * 0.058;
			$("#p_global").text(payload.toFixed());
		});
	},
	on_new_file: function(info) {
		FileList.add_info(info);
	},
	on_update_payload: function(item) {
		hash = item.hash;
		speed = item.value;
		speed = (speed * 0.058).toFixed(1);
		$("#"+hash).find(".speed").text(speed+"Mb/s");
	},
	on_update_progress: function(item) {
		hash = item.hash;
		progress = item.value;
		if (progress == 1) FileList.refresh_list();
		progress = (progress*100).toFixed(1) + "%";
			
		$("#"+hash).find(".progress").text(progress);
	}
};



function show_msg(msg, t) 
{
	$.noty.closeAll();
	noty({
			text: msg, 
			layout: "bottom",
			type: t || "alert",
			timeout: t || 5000
	});
}

function install_contextmenu()
{
	var do_delete = function(el) {
				show_msg(el.html());
				var h = el.attr("id");
				var name = el.find("span:eq(4)");
				rpc("filemanager", "del_file", {hash: h}, function(m) { 
					show_msg(m + " " + name.text());
				});
				$(el).remove();
	};
	var delete_item = function(el) {
		var name = el.find("span:eq(4)");
		$.noty.closeAll();
		noty({
				text: "确定要删除 " + name.text() + " 吗?",
				buttons: [
					{type: 'button green', text:"确定", 
						click: function(){do_delete(el);}},
					{type: 'button pink', text:"取消", 
						click: function(){}}
					],
					closable: false,
					timeout: false,
					layout: "center"
		});
	};
	var download_item = function(el) {
				var h = el.attr("id");
				rpc("filemanager", "download", {hash: h}, function(m) { 
					FileList.refresh_list();
				});
	};

	var items = $("#c_file_list li.item");

	items.has(".local1").contextMenu({menu: "local_menu"},
		function(action, el, pos) {
			if (action=="delete") { delete_item(el); }
		}
	);

	items.has(".local2").contextMenu({menu: "download_menu2"},
		function(action, el, pos) {
			if (action=="delete") { delete_item(el); }
		}
	);

	items.has(".remote").contextMenu({menu: "remote_menu"},
		function(action, el, pos) {
			if (action=="delete") { delete_item(el); }
			else if (action=="download") { download_item(el); }
		}
	);
}

$(function(){
	$("#menu").tabs();
	FileList.init();
	LocalFile.init();
	window.setInterval("Updater.update()", 1000);
	window.onunload = function() {
		rpc("systeminfo", "close_server", {});
	};
	window.onbeforeunload = function() {
		return "关闭此页面后后台程序将退出";
	};
	$.ajaxSetup({
		contentType: "application/x-www-form-urlencoded; charset=utf-8"
	});
});
