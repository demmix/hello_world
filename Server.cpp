#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <math.h>
#include <atomic>

// Для корректной работы freeaddrinfo в MinGW
// Подробнее: http://stackoverflow.com/a/20306451
#define _WIN32_WINNT 0x501

#include <WinSock2.h>
#include <WS2tcpip.h>

// Необходимо, чтобы линковка происходила с DLL-библиотекой
// Для работы с сокетам
#pragma comment(lib, "Ws2_32.lib")

using std::cerr;

std::atomic<bool> done[4]; // Use an atomic flag.

double Calc(std::string typeoperation, double first, double second)
{
	if (typeoperation == "add") {
		return (first + second);
	}
	else if (typeoperation == "sub")
	{
		return (first - second);
	}
	else if (typeoperation == "mul")
	{
		return (first * second);
	}
	else if (typeoperation == "div")
	{
		if (second == 0)
			return 0;

		return (first / second);
	}
	else if (typeoperation == "sqrt")
	{
		return std::sqrt(first);
	}
	else if (typeoperation == "pow")
	{
		return std::pow(first,second);
	}
	else if (typeoperation == "sin")
	{
		return std::sin(first);
	}
	else if (typeoperation == "cos")
	{
		return std::cos(first);
	}
	else if (typeoperation == "abs")
	{
		return std::abs(first);
	}
	else if (typeoperation == "arcsin")
	{
		return std::asin(first);
	}
	else if (typeoperation == "tan")
	{
		return std::tan(first);
	}
}

double Parse(std::string request)
{
	size_t to_start;
	size_t to_end;

	char typeOperation[] = "<TypeOperation>";
	char endTypeOperation[] = "</TypeOperation>";
	to_start = request.find(typeOperation) + strlen(typeOperation);
	to_end = request.find(endTypeOperation);
	std::string type = request.substr(to_start, to_end - to_start);

	char firstArg[] = "<FirstArg>";
	char endFirstArg[] = "</FirstArg>";
	to_start = request.find(firstArg) + strlen(firstArg);
	to_end = request.find(endFirstArg);
	std::string firstOp = request.substr(to_start, to_end - to_start);
	double first = std::stod(firstOp);

	char secondArg[] = "<SecondArg>";
	char endSecondArg[] = "</SecondArg>";
	to_start = request.find(secondArg) + strlen(secondArg);
	to_end = request.find(endSecondArg);
	std::string secondOp = request.substr(to_start, to_end - to_start);
	double second = std::stod(secondOp);

	return Calc(type, first, second);
}

void Process(int client_socket, int index)
{
	const int max_client_buffer_size = 1024;
	char buf[max_client_buffer_size];

	std::cout << "Get request " << index << std::endl;
	int result = recv(client_socket, buf, max_client_buffer_size, 0);
	std::stringstream response; // сюда будет записываться ответ клиенту
	std::stringstream response_body; // тело ответа

	if (result == SOCKET_ERROR) {
		// ошибка получения данных
		cerr << "recv failed: " << result << "\n";
		closesocket(client_socket);
	}
	else if (result == 0) {
		// соединение закрыто клиентом
		cerr << "connection closed...\n";
	}
	else if (result > 0) {
		// Мы знаем фактический размер полученных данных, поэтому ставим метку конца строки
		// В буфере запроса.
		buf[result] = '\0';



		std::string request = buf;

		// Данные успешно получены
		// формируем тело ответа (HTML)
		response_body << "<title>Test C++11 HTTP Server</title>\n"
			<< "<h1>Test page</h1>\n"
			<< "<p>This is body of the test page...</p>\n"
			<< "<h2>Request headers</h2>\n"
			<< "<pre>" << Parse(request) << "</pre>\n"
			<< "<em><small>Test C++ Http Server</small></em>\n";

		// Формируем весь ответ вместе с заголовками
		response << "HTTP/1.1 200 OK\r\n"
			<< "Version: HTTP/1.1\r\n"
			<< "Content-Type: text/html; charset=utf-8\r\n"
			<< "Content-Length: " << response_body.str().length()
			<< "\r\n\r\n"
			<< response_body.str();

		// Отправляем ответ клиенту с помощью функции send
		result = send(client_socket, response.str().c_str(),
			response.str().length(), 0);

		if (result == SOCKET_ERROR) {
			// произошла ошибка при отправле данных
			cerr << "send failed: " << WSAGetLastError() << "\n";
		}
		// Закрываем соединение к клиентом
		closesocket(client_socket);

		done[index] = true;
	}
}

int main()
{
	WSADATA wsaData; // служебная структура для хранение информации
					 // о реализации Windows Sockets
					 // старт использования библиотеки сокетов процессом
					 // (подгружается Ws2_32.dll)
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

	// Если произошла ошибка подгрузки библиотеки
	if (result != 0) {
		cerr << "WSAStartup failed: " << result << "\n";
		return result;
	}

	struct addrinfo* addr = NULL; // структура, хранящая информацию
								  // об IP-адресе  слущающего сокета

								  // Шаблон для инициализации структуры адреса
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_INET; // AF_INET определяет, что будет
							   // использоваться сеть для работы с сокетом
	hints.ai_socktype = SOCK_STREAM; // Задаем потоковый тип сокета
	hints.ai_protocol = IPPROTO_TCP; // Используем протокол TCP
	hints.ai_flags = AI_PASSIVE; // Сокет будет биндиться на адрес,
								 // чтобы принимать входящие соединения

								 // Инициализируем структуру, хранящую адрес сокета - addr
								 // Наш HTTP-сервер будет висеть на 8000-м порту локалхоста
	result = getaddrinfo("127.0.0.1", "8080", &hints, &addr);

	// Если инициализация структуры адреса завершилась с ошибкой,
	// выведем сообщением об этом и завершим выполнение программы
	if (result != 0) {
		cerr << "getaddrinfo failed: " << result << "\n";
		WSACleanup(); // выгрузка библиотеки Ws2_32.dll
		return 1;
	}

	// Создание сокета
	int listen_socket = socket(addr->ai_family, addr->ai_socktype,
		addr->ai_protocol);
	// Если создание сокета завершилось с ошибкой, выводим сообщение,
	// освобождаем память, выделенную под структуру addr,
	// выгружаем dll-библиотеку и закрываем программу
	if (listen_socket == INVALID_SOCKET) {
		cerr << "Error at socket: " << WSAGetLastError() << "\n";
		freeaddrinfo(addr);
		WSACleanup();
		return 1;
	}

	// Привязываем сокет к IP-адресу
	result = bind(listen_socket, addr->ai_addr, (int)addr->ai_addrlen);

	// Если привязать адрес к сокету не удалось, то выводим сообщение
	// об ошибке, освобождаем память, выделенную под структуру addr.
	// и закрываем открытый сокет.
	// Выгружаем DLL-библиотеку из памяти и закрываем программу.
	if (result == SOCKET_ERROR) {
		cerr << "bind failed with error: " << WSAGetLastError() << "\n";
		freeaddrinfo(addr);
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	// Инициализируем слушающий сокет
	if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
		cerr << "listen failed with error: " << WSAGetLastError() << "\n";
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	int index = 0;

	std::thread pool[4];


	int client_socket = INVALID_SOCKET;

	for (int i = 0; i < 4; i++)
		done[i] = true;

	for (;;) {
		// Принимаем входящие соединения
		client_socket = accept(listen_socket, NULL, NULL);
		if (client_socket == INVALID_SOCKET) {
			cerr << "accept failed: " << WSAGetLastError() << "\n";
			closesocket(listen_socket);
			WSACleanup();
			return 1;
		}

		if (done[0] == true) {
			pool[0] = std::thread(Process, client_socket, index);
			pool[0].detach();
			client_socket = INVALID_SOCKET;
		}
		else if (done[1] == true)
		{
			pool[1] = std::thread(Process, client_socket, index);
			pool[1].detach();
			client_socket = INVALID_SOCKET;
		}
		else if (done[2] == true)
		{
			pool[2] = std::thread(Process, client_socket, index);
			pool[2].detach();
			client_socket = INVALID_SOCKET;
		}
		else if (done[3] == true)
		{
			pool[3] = std::thread(Process, client_socket, index);
			pool[3].detach();
			client_socket = INVALID_SOCKET;
		}
		else
		{
			std::cout << "Do not free thread" << std::endl;
		}
	}

	// Убираем за собой
	closesocket(listen_socket);
	freeaddrinfo(addr);
	WSACleanup();
	return 0;
}