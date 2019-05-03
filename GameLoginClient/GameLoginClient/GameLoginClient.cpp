#include <iostream>
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/Outcome.h>
#include <aws/cognito-idp/model/AttributeType.h>
#include <aws/cognito-idp/model/SignUpRequest.h>
#include <aws/cognito-idp/model/SignUpResult.h>
#include <aws/cognito-idp/model/ConfirmSignUpRequest.h>
#include <aws/cognito-idp/model/ConfirmSignUpResult.h>
#include <aws/cognito-idp/model/InitiateAuthRequest.h>
#include <aws/cognito-idp/model/InitiateAuthResult.h>
#include <aws/cognito-idp/model/ForgotPasswordRequest.h>
#include <aws/cognito-idp/model/ForgotPasswordResult.h>
#include <aws/cognito-idp/model/ConfirmForgotPasswordRequest.h>
#include <aws/cognito-idp/model/ConfirmForgotPasswordResult.h>
#include <aws/cognito-idp/model/ChangePasswordRequest.h>
#include <aws/cognito-idp/model/ChangePasswordResult.h>
#include <aws/cognito-idp/model/GetUserRequest.h>
#include <aws/cognito-idp/model/GetUserResult.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/cognito-idp/CognitoIdentityProviderClient.h>

#include <WinSock2.h>
#include <WS2tcpip.h>
// WinSock includes windows.h which has some trouble some #defines
#undef min
#undef max
#undef GetMessage

#include "ClientSettings.h"

using namespace std;

const int RECV_BUFFER_LENGTH = 2048;
const char PORT[] = "27015";
const char SERVERADDR[] = "127.0.0.1";

static shared_ptr<Aws::CognitoIdentityProvider::CognitoIdentityProviderClient> s_AmazonCognitoClient;

static bool s_IsLoggedIn = false;
static string s_TokenType;
static string s_AccessToken;
static string s_IDToken;
static string s_RefreshToken;

bool SendAccessTokenToServer(const std::string& accessToken)
{
    cout << "Attempting to connect to server at " << SERVERADDR << ":" << PORT << endl;

    WSADATA wsaData;
    int errorNum = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (errorNum != 0)
    {
        cout << "WSAStartup failed with error " << errorNum << endl;
        return false;
    }

    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addrResult = nullptr;
    errorNum = getaddrinfo(SERVERADDR, PORT, &hints, &addrResult);
    if (errorNum != 0)
    {
        cout << "getaddrinfo failed with error " << errorNum << endl;
        WSACleanup();
        return false;
    }

    SOCKET connectSocket = INVALID_SOCKET;
    addrinfo* curAddr = nullptr;
    for (curAddr = addrResult; curAddr != nullptr; curAddr = curAddr->ai_next)
    {
        connectSocket = socket(curAddr->ai_family, curAddr->ai_socktype, curAddr->ai_protocol);
        if (connectSocket == INVALID_SOCKET)
        {
            cout << "Socket failed with error " << WSAGetLastError() << endl;
            WSACleanup();
            return false;
        }

        errorNum = connect(connectSocket, curAddr->ai_addr, (int)curAddr->ai_addrlen);
        // if can't connect to the given addrInfo, try the next one
        if (errorNum == INVALID_SOCKET)
        {
            closesocket(connectSocket);
            connectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(addrResult);

    if (connectSocket == INVALID_SOCKET)
    {
        cout << "Unable to connect with server" << endl;
        WSACleanup();
        return false;
    }

    // send key and wait for response
    int bytexfer = send(connectSocket, accessToken.c_str(), static_cast<int>(accessToken.length()), 0);
    if (bytexfer == SOCKET_ERROR)
    {
        cout << "Send access token failed due to error " << WSAGetLastError() << endl;
        closesocket(connectSocket);
        WSACleanup();
        return false;
    }

    cout << "Token bytes sent: " << bytexfer << endl;

    char recvBuffer[RECV_BUFFER_LENGTH];

    do
    {
        bytexfer = recv(connectSocket, recvBuffer, RECV_BUFFER_LENGTH, 0);
        if (bytexfer > 0)
        {
            cout << "Bytes received: " << bytexfer << endl;
            cout.write(recvBuffer, bytexfer);
            cout << endl;

            std::string serverResponse{ recvBuffer, recvBuffer + bytexfer };
            cout << "Server response: " << serverResponse << endl;
            break;
        }
        else if (bytexfer == 0)
        {
            cout << "Connection closed" << endl;
        }
        else
        {
            cout << "Error receiving data: " << WSAGetLastError() << endl;
        }
    
        Sleep(5);
    } while (bytexfer > 0);

    closesocket(connectSocket);
    WSACleanup();

    return true;
}

void LogIn()
{
    string username;
    cout << "username: ";
    cin >> username;

    string password;
    cout << "password: ";
    cin >> password;

    Aws::CognitoIdentityProvider::Model::InitiateAuthRequest initiateAuthRequest;
    initiateAuthRequest.SetClientId(APP_CLIENT_ID);
    initiateAuthRequest.SetAuthFlow(Aws::CognitoIdentityProvider::Model::AuthFlowType::USER_PASSWORD_AUTH);

    map<string, string> authParameters{
        {"USERNAME", username},
        {"PASSWORD", password}
    };
    initiateAuthRequest.SetAuthParameters(authParameters);

    Aws::CognitoIdentityProvider::Model::InitiateAuthOutcome initiateAuthOutcome{ s_AmazonCognitoClient->InitiateAuth(initiateAuthRequest) };
    if (initiateAuthOutcome.IsSuccess())
    {
        Aws::CognitoIdentityProvider::Model::InitiateAuthResult initiateAuthResult{ initiateAuthOutcome.GetResult() };
        if (initiateAuthResult.GetChallengeName() == Aws::CognitoIdentityProvider::Model::ChallengeNameType::NOT_SET)
        {
            // for this code sample, this is what we expect, there should be no further challenges
            // there are more complex options, for example requiring the user to reset the password the first login
            // or using a more secure password transfer mechanism which will be covered in later examples
            Aws::CognitoIdentityProvider::Model::AuthenticationResultType authenticationResult = initiateAuthResult.GetAuthenticationResult();
            cout << endl << "Congratulations, you have successfully signed in!" << endl;
            cout << "\tToken Type: " << authenticationResult.GetTokenType() << endl;
            cout << "\tAccess Token: " << authenticationResult.GetAccessToken().substr(0, 20) << " ..." << endl;
            cout << "\tExpires in " << authenticationResult.GetExpiresIn() << " seconds" << endl;
            cout << "\tID Token: " << authenticationResult.GetIdToken().substr(0, 20) << " ..." << endl;
            cout << "\tRefresh Token: " << authenticationResult.GetRefreshToken().substr(0, 20) << " ..." << endl;

            s_IsLoggedIn = true;
            s_TokenType = authenticationResult.GetTokenType();
            s_AccessToken = authenticationResult.GetAccessToken();
            s_IDToken = authenticationResult.GetIdToken();
            s_RefreshToken = authenticationResult.GetRefreshToken();

            if (!SendAccessTokenToServer(s_AccessToken))
            {
                cout << "Unable to connect to server" << endl;
            }
        }
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = initiateAuthOutcome.GetError();
        cout << "Error logging in: " << error.GetMessage() << endl << endl;
    }
}

void SignUp()
{
    string username;
    cout << "What user name would you like to use for your account? ";
    cin >> username;

    string password;
    string checkPassword;
    while (password.empty())
    {
        // this will obviously show the password in clear text so in non-sample code
        // please use a secure method to allow the user type their password
        cout << endl << "Passwords require 8 characters minimum, at least one number, " << endl << "at least one special character and at least one each of capital and lower case." << endl;
        cout << "What password would you like to use? ";
        cin >> password;

        cout << "Please type your password again: ";
        cin >> checkPassword;

        if (password != checkPassword)
        {
            cout << endl << "Passwords don't match, try again." << endl;
            password.clear();
            checkPassword.clear();
        }
    }
    string email;
    cout << "What e-mail address would you like to use to verify your registration? ";
    cin >> email;

    cout << endl << "One moment while I process your registration...";

    Aws::CognitoIdentityProvider::Model::SignUpRequest signUpRequest;
    signUpRequest.SetClientId(APP_CLIENT_ID);
    signUpRequest.SetUsername(username);
    signUpRequest.SetPassword(password);

    // note that options, like the e-mail address requirement, are stored in an attributes vector
    // not exposed through the request like required fields.
    Aws::Vector<Aws::CognitoIdentityProvider::Model::AttributeType> attributes;
    Aws::CognitoIdentityProvider::Model::AttributeType emailAttribute;
    emailAttribute.SetName("email");
    emailAttribute.SetValue(email);
    attributes.push_back(emailAttribute);
    signUpRequest.SetUserAttributes(attributes);

    Aws::CognitoIdentityProvider::Model::SignUpOutcome signUpOutcome{ s_AmazonCognitoClient->SignUp(signUpRequest) };
    if (signUpOutcome.IsSuccess())
    {
        Aws::CognitoIdentityProvider::Model::SignUpResult signUpResult{ signUpOutcome.GetResult() };
        cout << "Successful signup!" << endl;
        cout << "Details:" << endl;
        cout << "\tUser Confirmed: " << signUpResult.GetUserConfirmed() << endl;
        cout << "\tUser UUID: " << signUpResult.GetUserSub() << endl << endl;
        cout << "Please check your e-mail for confirmation code then enter the code from the confirmation menu option." << endl;
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = signUpOutcome.GetError();
        cout << "Error signing up: " << error.GetMessage() << endl << endl;
    }
}

void ConfirmRegistration()
{
    cout << "Please type the user name of account you'd like to confirm: ";
    string userName;
    cin >> userName;
    
    cout << "Please type the confirmation code for your account: ";
    string confirmationCode;
    cin >> confirmationCode;

    Aws::CognitoIdentityProvider::Model::ConfirmSignUpRequest confirmSignupRequest;
    confirmSignupRequest.SetClientId(APP_CLIENT_ID);
    confirmSignupRequest.SetUsername(userName);
    confirmSignupRequest.SetConfirmationCode(confirmationCode);

    Aws::CognitoIdentityProvider::Model::ConfirmSignUpOutcome confirmSignupOutcome { s_AmazonCognitoClient->ConfirmSignUp(confirmSignupRequest) };
    if (confirmSignupOutcome.IsSuccess())
    {
        cout << "Confirmation succeeded! You can now log in from the menu." << endl << endl;
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = confirmSignupOutcome.GetError();
        cout << "Error confirming signup: " << error.GetMessage() << endl << endl;
    }
}

void ForgotPassword()
{
    string username;
    cout << "I forgot the password for username: ";
    cin >> username;

    Aws::CognitoIdentityProvider::Model::ForgotPasswordRequest forgotPasswordRequest;
    forgotPasswordRequest.SetClientId(APP_CLIENT_ID);
    forgotPasswordRequest.SetUsername(username);

    Aws::CognitoIdentityProvider::Model::ForgotPasswordOutcome forgotPasswordOutcome{ s_AmazonCognitoClient->ForgotPassword(forgotPasswordRequest) };
    if (forgotPasswordOutcome.IsSuccess())
    {
        cout << "Password reset accepted. Check your email for a reset code and input it: ";
        string confirmationCode;
        cin >> confirmationCode;

        cout << "New password: ";
        string password;
        cin >> password;

        Aws::CognitoIdentityProvider::Model::ConfirmForgotPasswordRequest confirmForgotPasswordRequest;
        confirmForgotPasswordRequest.SetClientId(APP_CLIENT_ID);
        confirmForgotPasswordRequest.SetUsername(username);
        confirmForgotPasswordRequest.SetConfirmationCode(confirmationCode);
        confirmForgotPasswordRequest.SetPassword(password);

        Aws::CognitoIdentityProvider::Model::ConfirmForgotPasswordOutcome confirmForgotPasswordOutcome{ s_AmazonCognitoClient->ConfirmForgotPassword(confirmForgotPasswordRequest) };
        if (confirmForgotPasswordOutcome.IsSuccess())
        {
            cout << "Confirmation succeeded! You can now log in from the menu." << endl << endl;
        }
        else
        {
            Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = confirmForgotPasswordOutcome.GetError();
            cout << "Error confirming password reset: " << error.GetMessage() << endl << endl;
        }
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = forgotPasswordOutcome.GetError();
        cout << "Forgot password request error: " << error.GetMessage() << endl << endl;
    }

}

void ChangePassword()
{
    if (!s_IsLoggedIn)
    {
        cout << "Please log in before trying to change your password." << endl;
        return;
    }
    
    cout << "Please type the current password: ";
    string currentPassword;
    cin >> currentPassword;

    cout << "Please type the new password: ";
    string newPassword;
    cin >> newPassword;

    Aws::CognitoIdentityProvider::Model::ChangePasswordRequest changePasswordRequest;
    changePasswordRequest.SetAccessToken(s_AccessToken);
    changePasswordRequest.SetPreviousPassword(currentPassword);
    changePasswordRequest.SetProposedPassword(newPassword);

    Aws::CognitoIdentityProvider::Model::ChangePasswordOutcome changePasswordOutcome{ s_AmazonCognitoClient->ChangePassword(changePasswordRequest) };
    if (changePasswordOutcome.IsSuccess())
    {
        cout << "Password reset successful!" << endl << endl;
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = changePasswordOutcome.GetError();
        cout << "Error changing password: " << error.GetMessage() << endl << endl;
    }
}

void DisplayInfo()
{
    if (!s_IsLoggedIn)
    {
        cout << "Please log in before trying to view your user information." << endl;
        return;
    }

    Aws::CognitoIdentityProvider::Model::GetUserRequest getUserRequest;
    getUserRequest.SetAccessToken(s_AccessToken);

    Aws::CognitoIdentityProvider::Model::GetUserOutcome getUserOutcome{ s_AmazonCognitoClient->GetUser(getUserRequest) };
    if (getUserOutcome.IsSuccess())
    {
        Aws::CognitoIdentityProvider::Model::GetUserResult getUserResult{ getUserOutcome.GetResult() };
        cout << endl << "User name : " << getUserResult.GetUsername() << endl;

        for (const auto& attribute : getUserResult.GetUserAttributes())
        {
            cout << attribute.GetName() << " : " << attribute.GetValue() << endl;
        }
    }
    else
    {
        Aws::Client::AWSError<Aws::CognitoIdentityProvider::CognitoIdentityProviderErrors> error = getUserOutcome.GetError();
        cout << "Error getting user: " << error.GetMessage() << endl << endl;
    }

}

bool LoggedOutMenu()
{
    cout << endl << "What would you like to do?" << endl;
    cout << "\t1. Log in" << endl;
    cout << "\t2. Sign up" << endl;
    cout << "\t3. Confirm registration" << endl;
    cout << "\t4. I forgot my password" << endl;
    cout << "\t5. Quit" << endl;
    cout << endl << "Your choice? ";

    int menuSelection = 0;
    if (!(cin >> menuSelection))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    switch (menuSelection)
    {
    case 1:
        LogIn();
        break;

    case 2:
        SignUp();
        break;

    case 3:
        ConfirmRegistration();
        break;

    case 4:
        ForgotPassword();
        break;

    case 5:
        return false;

    default:
        cout << "That choice doesn't exist, please try again." << endl << endl;
    }
    return true;
}

void LogOut()
{
    s_IsLoggedIn = false;
    s_TokenType.clear();
    s_AccessToken.clear();
    s_IDToken.clear();
    s_RefreshToken.clear();
}

bool LoggedInMenu()
{
    cout << endl << "What would you like to do?" << endl;
    cout << "\t1. See my info" << endl;
    cout << "\t2. Change my password" << endl;
    cout << "\t3. Log out" << endl;
    cout << "\t4. Quit" << endl;
    cout << endl << "Your choice? ";

    int menuSelection = 0;
    if (!(cin >> menuSelection))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    switch (menuSelection)
    {
    case 1:
        DisplayInfo();
        break;

    case 2:
        ChangePassword();
        break;

    case 3:
        LogOut();
        break;

    case 4:
        return false;

    default:
        cout << "That choice doesn't exist, please try again." << endl << endl;
    }
    return true;
}

int RunMainLoop()
{
    cout << "Welcome to The Game!" << endl;

    bool keepRunning = true;
    while (keepRunning)
    {
        if (s_IsLoggedIn)
        {
            keepRunning = LoggedInMenu();
        }
        else
        {
            keepRunning = LoggedOutMenu();
        }
    }
    return 0;
}

int main()
{
    Aws::SDKOptions options;

    Aws::Utils::Logging::LogLevel logLevel{ Aws::Utils::Logging::LogLevel::Error };
    options.loggingOptions.logger_create_fn = [logLevel] {return make_shared<Aws::Utils::Logging::ConsoleLogSystem>(logLevel); };

    Aws::InitAPI(options);

    Aws::Client::ClientConfiguration clientConfiguration;
    clientConfiguration.region = REGION;    // region must be set for Cognito operations

    s_AmazonCognitoClient = Aws::MakeShared<Aws::CognitoIdentityProvider::CognitoIdentityProviderClient>("CognitoIdentityProviderClient", clientConfiguration);

    int exitStatus = RunMainLoop();

    Aws::ShutdownAPI(options);
    return exitStatus;
}

