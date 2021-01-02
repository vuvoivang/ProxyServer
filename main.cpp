#include "proxy.h"
//Socket dùng để lắng nghe các truy cập mới
SOCKET socListen;
bool isRun = true;
vector<string> black_list;
//Chuỗi trả về khi nằm trong black list
string res403;

void runProxyServer()
{
	WSADATA wsaData;// Biến lưu thông tin của việc cài đặt window socket
	sockaddr_in address;// Biến lưu địa chỉ ip và cổng dành cho socket
	SOCKET ListenSocket = INVALID_SOCKET;
	// Cài đặt các Socket
	// Khởi tạo winsock WSAStartup(version, pointer to WSADATA)
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0)
	{
		cout << "\nWinsock initialize failed " << err << endl;
		WSACleanup();
		return;
	}
	//
	address.sin_family = AF_INET;// họ của socket là ipv4 
	address.sin_addr.s_addr = INADDR_ANY;//chứa địa chỉ ipv4, INADDR_ANY: tất cả các interfaces
	address.sin_port = htons(PORT);// htons chuyển đổi số sang byte theo TCP/IP 
	ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	// Khởi tạo socket
	if (ListenSocket == INVALID_SOCKET)
	{
		cout << "\nSocket initialize failed";
		WSACleanup();
		return;
	}
	// Bind Socket: ràng buộc socket với Interfaces có sẵn
	// Vì là server nên cần kết nối tới tất cả interfaces (khai báo INADDR_ANY ở trên)
	if (bind(ListenSocket, (sockaddr*)&address, sizeof(address)) != 0)
	{
		cout << "\nSocket bind failed";
		WSACleanup();
		return;
	};
	//Bắt đầu lắng nghe các truy cập
	if (listen(ListenSocket, MAXSOCCONNECT) != 0)
	{
		cout << "\n Socket listening function failed";
		WSACleanup();
		return;
	}
	socListen = ListenSocket;
	if (!LoadBlackList(black_list)) {
		cout << "Can't open blacklist file" << endl;
	}
	loadHTMLForbidden(res403);
	if (res403.empty())
	{
		cout << "Can't load HTMl file" << endl;
	}
	AfxBeginThread(clientConnectToProxy, (LPVOID)ListenSocket);
	CWinThread* p = AfxBeginThread(StopProcess, &isRun);
}

UINT clientConnectToProxy(void* longParam)
{
	ProxyServer proxy;
	SOCKET socket = (SOCKET)longParam,SocClient;
	sockaddr_in addr_in;
	int addrLen = sizeof(addr_in);
	// Chấp nhận yêu cầu kết nối từ client
	SocClient = accept(socket, (sockaddr*)&addr_in, &addrLen);
	cout << "\nClient has Connected" << endl;
	//Tạo 1 thread để lắng nghe từ client
	AfxBeginThread(clientConnectToProxy, longParam);
	char* Buffer = new char[MAXSIZE];
	proxy.serverStatus = closed;
	proxy.clientStatus = opened;
	proxy.Client = SocClient;
	//Nhận truy vấn gởi tới từ Client
	int return_val = recv(proxy.Client, Buffer, MAXSIZE, 0);
	if (return_val == SOCKET_ERROR) {
		cout << "\nError in receiving request from client" << endl;
		if (proxy.clientStatus == opened) {
			closesocket(proxy.Client);
			proxy.clientStatus = closed;
			/*WSAGetLastError();*/
		}
	}
	if (return_val == 0) // Khi kết nối đóng, trả về 0
	{
		cout << "\nClient has disconnected" << endl;
		if (proxy.clientStatus == opened) {
			closesocket(proxy.Client);
			proxy.clientStatus = closed;
		}
	}
	handleBuffer(Buffer, return_val);
	//Khi nhận truy vấn thành công,xuất ra nội dung
	cout << "\nTotal bytes received : " << return_val << endl;
	cout << "Content : " << endl  << Buffer << endl;
	Sleep(2000); 
	string tempBuf = Buffer, address;
	int port = HTTPPORT;
	getAddress(tempBuf, address);
	bool check = false;
	if (CheckUrlForbidden(address)) {
		//Nếu địa chỉ đó nằm trong blacklist thì sẽ báo lỗi, không gửi truy vấn tới server
		return_val = send(proxy.Client, res403.c_str(), res403.size(), 0);
		if(address != "") cout << "\nBlock : " << address << endl;
		check = true;
		Sleep(5000);
	}
	if (check == false) {
		Query query;
		query.handle = CreateEvent(NULL, TRUE, FALSE, NULL);
		query.address = address;
		query.port = port;
		query.pair = &proxy;
		//Tạo thread giao tiếp giữa Proxy Server và Remote Server
		CWinThread* pThread = AfxBeginThread(proxyConnectToRemoteServer, (LPVOID)&query);
		//Đợi cho Proxy kết nối tới Server
		WaitForSingleObject(query.handle, 5000);
		CloseHandle(query.handle);
		while (proxy.clientStatus == opened
			&& proxy.serverStatus == opened) {
			//Proxy gởi truy vấn
			return_val = send(proxy.Server, tempBuf.c_str(), tempBuf.size(), 0);
			if (return_val == SOCKET_ERROR) {
				cout << "Error in sending : " << GetLastError();
				if (proxy.serverStatus == opened) {
					closesocket(proxy.Server);
					proxy.serverStatus = closed;
				}
				continue;
			}
			//Vòng lặp tiếp tục nhận các truy vấn từ Client đến khi nhận hết thì 1 trong 2 Client và Server sẽ ngắt kết nối
			return_val = recv(proxy.Client, Buffer, MAXSIZE, 0);
			if (return_val == SOCKET_ERROR) {
				cout << "Error in receving from server : " << GetLastError();
				if (proxy.clientStatus == opened) {
					closesocket(proxy.Client);
					proxy.clientStatus = closed;
				}
				continue;
			}
			if (return_val == 0) {
				cout << "Client closed " << endl;
				if (proxy.clientStatus == opened) {
					closesocket(proxy.Server);
					proxy.clientStatus = closed;
				}
				break;
			}
			handleBuffer(Buffer, return_val);
			cout << "\nTotal bytes received : " << return_val << endl;
			cout << "Content : " << endl << Buffer << endl;
			Sleep(5000);
		}
		// Nếu 1 trong 2 trạng thái còn opened thì đóng nó lại
		if (proxy.serverStatus == opened) {
			closesocket(proxy.Server);
			proxy.serverStatus = closed;
		}
		if (proxy.clientStatus == opened) {
			closesocket(proxy.Client);
			proxy.clientStatus = closed;
		}
		WaitForSingleObject(pThread->m_hThread, 20000);
	}
	else // Nếu lỗi 403 mà client còn opened thì bật closed
	{
		if (proxy.clientStatus == opened) {
			closesocket(proxy.Client);
			proxy.clientStatus = closed;
		}
	}
	delete[] Buffer;
	return 0;

}

UINT proxyConnectToRemoteServer(void* longQuery)
{
	char hostname[40] = "";
	Query* query = (Query*)longQuery;
	string serverName = query->address;
	int port = query->port;
	sockaddr_in* server = NULL;
	cout << "\nServer Name: " << serverName << endl;
	server = GetServer(serverName, hostname); // Get server của truy vấn 
	if (server == NULL) {
		cout << "\n Can not get IP" << endl;
		send(query->pair->Client, res403.c_str(), res403.size(), 0);
		return -1;
	}
	if (strlen(hostname) > 0) {		// Đã get server thành công
		cout << "Connecting to:" << hostname << endl;
		int return_val;
		char* Buffer = new char[MAXSIZE];
		SOCKET Server = socket(AF_INET, SOCK_STREAM, 0);	//Khỏi tạo Socket
		//Kết nối tới địa chỉ IP vừa lấy được
		if (!(connect(Server, (sockaddr*)server, sizeof(sockaddr)) == 0)) {
			cout << "Can not connect";
			send(query->pair->Client, res403.c_str(), res403.size(), 0);
			return -1;
		}
		else {
			cout << "Successful Connection \n";
			query->pair->Server = Server;
			query->pair->serverStatus = opened;
			SetEvent(query->handle);	//‎ngăn chặn một số threads khỏi việc đọc từ bộ đệm bộ nhớ được chia sẻ trong khi một thread đang ghi cho bộ đệm đó‎

			while (query->pair->clientStatus == opened &&
				query->pair->serverStatus == opened) {
				//Nhận data gởi từ Server tới Proxy
				return_val = recv(query->pair->Server, Buffer, MAXSIZE, 0);
				if (return_val == SOCKET_ERROR) {
					closesocket(query->pair->Server);
					query->pair->serverStatus = closed;
					break;
				}
				if (return_val == 0) {
					cout << "\nServer Closed" << endl;
					closesocket(query->pair->Server);
					query->pair->serverStatus = closed;
				}
				//Gởi data đó tới Client của proxy
				//Kết thúc vòng lặp khi đã nhận và gởi hết data
				return_val = send(query->pair->Client, Buffer, return_val, 0);
				if (return_val == SOCKET_ERROR) {
					cout << "\nSend Failed, Error: " << GetLastError();
					closesocket(query->pair->Client);
					query->pair->clientStatus = closed;
					break;
				}
				handleBuffer(Buffer, return_val);
				cout << "\nTotal bytes server received : " << return_val << endl; 
				cout << "Content : " << endl << Buffer << endl;
				ZeroMemory(Buffer, MAXSIZE);
			}
			//Đóng socket
			//Việc thay đổi giá trị ở thread này sẽ ảnh hưởng tới thread clientConnectToProxy
			//Việc đóng Socket ở thread này => các giá trị thread kia cũng thay đổi
			if (query->pair->clientStatus == opened) {
				closesocket(query->pair->Client);
				query->pair->clientStatus = closed;
			}
			if (query->pair->serverStatus == opened) {
				closesocket(query->pair->Server);
				query->pair->serverStatus = closed;
			}
		}
		delete[] Buffer;
	}

	return 0;
}

void shutDownServer()
{
	//Đóng Socket lắng nghe
	cout << "Socket Listen has already closed" << endl;
	closesocket(socListen);
	WSACleanup();
}

void handleBuffer(char* Buf, int return_val)
{
	if (return_val >= MAXSIZE)
	{
		Buf[MAXSIZE - 1] = '\0';
	}
	else if (return_val > 0) Buf[return_val] = '\0';
	else Buf[0] = '\0';


}
void getAddress(string& buf, string& address)
{
	vector<string> temp;
	/* temp[0] : GET/POST
	   temp[1] : URL
	   temp[2] : protocol
	*/
	SplitData(buf, temp);
	if (temp.size() > 0) {
		int pos = temp[1].find(HTTP);
		if (pos >= 0) {
			int start = pos + strlen(HTTP);
			address = temp[1].substr(start, temp[1].find('/', start) - start);
			// address : www.xxx.com

			//chuyển buf từ GET http://www.xxx.com/...	 HTTP1.0
			//thành			GET					  /...	 HTTP1.0
			string temp2;
			int len = strlen(HTTP) + address.length();
			while (len--)
				temp2.push_back(' ');
			buf = buf.replace(buf.find(HTTP + address), strlen(HTTP) + address.length(), temp2);

		}
	}
}
sockaddr_in* GetServer(string serverName, char* hostname)
{
	sockaddr_in* server = NULL;
	if (serverName.size() > 0) {
		int check;
		//Kiểm tra xem địa chỉ lấy được ở địa dạng nào www.xxx.com hay 192.168...
		if (isalpha(serverName[0])) {
			addrinfo hints, * res = NULL;
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			//Lấy thông tin từ địa chỉ lấy được 
			check = getaddrinfo(serverName.c_str(), "80", &hints, &res);
			if (check != 0) {
				cout << "Function getaddrinfo error : " << gai_strerror(check) << endl;
				return NULL;
			}
			while (res->ai_next != NULL) {
				res = res->ai_next;
			}
			sockaddr_in* temp = (sockaddr_in*)res->ai_addr;
			inet_ntop(res->ai_family, &temp->sin_addr, hostname, 32);
			server = (sockaddr_in*)res->ai_addr;
			unsigned long addr;
			inet_pton(AF_INET, hostname, &addr);
			server->sin_addr.s_addr = addr;
			server->sin_port = htons(HTTPPORT);
			server->sin_family = AF_INET;
		}
		else {
			unsigned long addr;
			inet_pton(AF_INET, serverName.c_str(), &addr);
			sockaddr_in sa;
			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = addr;
			if ((check = getnameinfo((sockaddr*)&sa,
				sizeof(sockaddr), hostname, NI_MAXHOST, NULL, NI_MAXSERV, NI_NUMERICSERV)) != 0) {
				cout << "Error";
				return NULL;
			}
			server->sin_addr.s_addr = addr;
			server->sin_family = AF_INET;
			server->sin_port = htons(HTTPPORT);
		}
	}

	return server;
}
void SplitData(string Buf, vector<string>& temp)
{
	istringstream stringStream(Buf);
	string token;
	while (getline(stringStream, token, ' ')) {
		temp.push_back(token);
	}
}
bool LoadBlackList(vector<string>& arr)
{
	ifstream f("blacklist.conf", ios::in);
	if (f.is_open()) {
		string temp;
		while (getline(f, temp)) {
			if (temp.back() == '\n') // Nếu ký tự cuối xuống dòng thì bỏ nó
			{
				temp.pop_back();
			}
			arr.push_back(temp);
		}
		f.close();
		return true;
	}
	else return false;
}
bool CheckUrlForbidden(string serverName) {

	int size = black_list.size();
	if (size > 0) {
		for (int i = 0; i < size; i++)
		{
			if (black_list[i].find(serverName) != string::npos) {
				return true;
			}
		}
	}
	return false;
}
void loadHTMLForbidden(string& res403)
{
	ifstream f("403_error.txt", ios::in);
	if (f.is_open())
	{
		getline(f, res403);
		f.close();
	}
}
UINT StopProcess(void* param)
{
	bool* run = (bool*)param;
	while (*run) {
		char c;
		c = getchar();
		if (c == 'e') { //Khi nhấn phím e(exist) thì thoát
			*run = 0;
		}
	}
	return 0;
}







int main()
{
	// Khởi tạo thư viện MFC
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		cout << "Error : MFC initialization failed " << endl;
	}
	else
	{
		// Khởi tạo thành công thì bắt đầu chương trình
		runProxyServer();
		while (isRun) {
			Sleep(10000);
		}
		shutDownServer();
	}

	return 1;
}