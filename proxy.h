#pragma once
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS    
// some CString constructors will be explicit
#define _AFX_NO_MFC_CONTROLS_IN_DIALOGS       
// remove support for MFC controls in dialogs
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif
#include <afx.h>
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>                     // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
// TODO: reference additional headers your program requires here
#include "define.h"
#include <stdio.h>
#include <tchar.h>
#include <SDKDDKVer.h> // Including SDKDDKVer.h defines the highest available Windows platform.
#include "afxsock.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
using namespace std;
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
struct ProxyServer {
	SOCKET Server;
	SOCKET Client;
	bool serverStatus;
	bool clientStatus;
};
//struct này chứa các thông tin của một truy vấn 
struct Query {
	string address; // địa chỉ
	HANDLE handle; // biến xử lí
	ProxyServer* pair; //một cặp giao tiếp
	int port; //port
};
// Hàm tạo proxy server và socket listen, lắng nghe các truy cập
void runProxyServer();

// Tạo luồng kết nối và giao tiếp giữa client và proxy server (kết nối,lắng nghe,nhận truy vấn,xuất truy vấn...)
// Mặt khác nó còn xử lí blacklist để chặn web, tạo vòng lặp liên tục nhận truy vấn
// từ client rồi tạo thread gửi đến remote server
UINT clientConnectToProxy(void* longParam);

// Tạo luồng kết nối và giao tiếp giữa proxy server và remote server(lấy địa chỉ IP và kết nối tới remote server)
// Tạo vòng lặp liên tục nhận dữ liệu
// từ remote server rồi gửi đến client của proxy
UINT proxyConnectToRemoteServer(void* longQuery);

//Đóng socket listen và winsock
void shutDownServer();

// Hàm xử lí dữ liệu gửi về
void handleBuffer(char* Buf, int return_val);

//Lấy địa chỉ trang web từ dữ liệu client gửi
void getAddress(string& buf, string& address);

//Từ serverName, get địa chỉ IP để kết nối tới remote server
sockaddr_in* GetServer(string serverName, char* hostname);

//Tách từ chuỗi request của client thành mảng 3 phần từ bao gồm :
/*  GET/POST
	URL
	protocol
*/
void SplitData(string Buf, vector<string>& temp);

//Đọc file blacklist để chặn web
bool LoadBlackList(vector<string>& arr);
//Trả về true nếu serverName có trong blacklist
bool CheckUrlForbidden(string serverName);
// Hàm load file HTML báo lỗi 403
void loadHTMLForbidden(string& res403); 

//Khi người dùng muốn dừng chương trình, nhấn phím e(exit)
UINT StopProcess(void* param);








