#include <iostream>
#include <memory>
#include <map>

#include <aws/core/Aws.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/cognito-idp/model/GetUserRequest.h>
#include <aws/cognito-idp/model/GetUserResult.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/cognito-idp/CognitoIdentityProviderClient.h>

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#undef GetMessage

#include "ServerSettings.h"

const int RECV_BUFFER_LENGTH = 2048;
const char PORT[] = "27015";

static std::shared_ptr<Aws::CognitoIdentityProvider::CognitoIdentityProviderClient> s_AmazonCognitoClient;

bool IsUserValid(const std::string& accessToken)
{
    Aws::CognitoIdentityProvider::Model::GetUserRequest getUserRequest;
    getUserRequest.SetAccessToken(accessToken);

    Aws::CognitoIdentityProvider::Model::GetUserOutcome getUserOutcome{ s_AmazonCognitoClient->GetUser(getUserRequest) };
    if (getUserOutcome.IsSuccess())
    {
        Aws::CognitoIdentityProvider::Model::GetUserResult getUserResult{ getUserOutcome.GetResult() };
        std::cout << "User logged in : " << getUserResult.GetUsername() << std::endl;

        for (const auto& attribute : getUserResult.GetUserAttributes())
        {
            std::cout << attribute.GetName() << " : " << attribute.GetValue() << std::endl;
        }
        return true;
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = getUserOutcome.GetError();
        std::cout << "Error getting user: " << error.GetMessage() << std::endl ;
        return false;
    }
}

int main()
{
    Aws::SDKOptions options;
    Aws::Utils::Logging::LogLevel logLevel{ Aws::Utils::Logging::LogLevel::Error };
    options.loggingOptions.logger_create_fn = [logLevel] {return std::make_shared<Aws::Utils::Logging::ConsoleLogSystem>(logLevel); };
    Aws::InitAPI(options);

    Aws::Client::ClientConfiguration clientConfiguration;
    clientConfiguration.region = REGION;    // region must be set for Cognito operations

    s_AmazonCognitoClient = Aws::MakeShared<Aws::CognitoIdentityProvider::CognitoIdentityProviderClient>("CognitoIdentityProviderClient", clientConfiguration);

    WSADATA wsaData;
    int errorNum;
 
    errorNum = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorNum != 0)
    {
        std::cout << "WSAStartup failed with error " << errorNum << std::endl;
        return 1;
    }

    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* addrResult = nullptr;
    errorNum = getaddrinfo(nullptr, PORT, &hints, &addrResult);
    if (errorNum != 0)
    {
        std::cout << "getaddrinfo failed with error " << errorNum << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cout << "Socket failed with error " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrResult);
        WSACleanup();
        return 1;
    }
    
    errorNum = bind(listenSocket, addrResult->ai_addr, static_cast<int>(addrResult->ai_addrlen));
    freeaddrinfo(addrResult);
    if (errorNum == SOCKET_ERROR)
    {
        std::cout << "Socket failed with error " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    errorNum = listen(listenSocket, SOMAXCONN);
    if (errorNum == SOCKET_ERROR)
    {
        std::cout << "listen failed with error " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Listening on port " << PORT << std::endl;

    SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
    closesocket(listenSocket);

    std::cout << "Connecting" << std::endl;

    if (clientSocket == INVALID_SOCKET)
    {
        std::cout << "accept failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    int sendResult = 0;
    int recvResult = 0;
    char recvBuffer[RECV_BUFFER_LENGTH];
 
    do
    {
        recvResult = recv(clientSocket, recvBuffer, RECV_BUFFER_LENGTH, 0);
        if (recvResult > 0)
        {
            std::cout << "Bytes received from client: " << recvResult << std::endl;
            std::cout.write(recvBuffer, recvResult);
            std::cout << std::endl;

            std::string accessToken{ recvBuffer, recvBuffer + recvResult };

            std::string message = IsUserValid(accessToken) ? "Login Success" : "Login Fail";

            sendResult = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
            if (sendResult == SOCKET_ERROR)
            {
                std::cout << "Send failed due to error: " << WSAGetLastError() << std::endl;
            }
            std::cout << "Bytes sent: " << sendResult << std::endl;
        }
        else if (recvResult == 0)
        {
            std::cout << "Closing connection..." << std::endl;
        }
        else
        {
            std::cout << "recv failed with error " << WSAGetLastError() << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }
        Sleep(500);
    } while (recvResult > 0);

    errorNum = shutdown(clientSocket, SD_SEND);
    if (errorNum == SOCKET_ERROR)
    {
        std::cout << "shutdown failed with error " << WSAGetLastError() << std::endl;
    }

    closesocket(clientSocket);
    WSACleanup();

    Aws::ShutdownAPI(options);

    return errorNum == SOCKET_ERROR ? 1 : 0;
}

