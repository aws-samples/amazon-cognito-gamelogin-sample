# amazon-cognito-gamelogin-sample
A basic demo of logging in to a game server with Amazon Cognito. The client and server use a socket to communicate a key obtained from Amazon Cognito.

# Requirements
- An AWS account with access to Amazon Cognito: https://aws.amazon.com/getting-started/
- Microsoft Visual Studio 2017: https://visualstudio.microsoft.com/vs/older-downloads/

# Contents
<pre>
├── GameLoginServer/         # Contains the server project and source code
└── GameLoginClient/         # Contains solution file as well as the client project
    ├── GameLoginClient.sln  # Solution that builds both the client and server
    └── GameLoginClient/     # Contains the client project and source code
</pre>

# Building and using the sample
1. Create a new Amazon Cognito User Pool through the AWS console. In the app client details be sure to enable "username-password (non-SRP) flow for app-based authentication (USER_PASSWORD_AUTH)". 
2. Modify the ClientSettings.h and ServerSettings.h file. The app client ID will get pasted in the client settings. Make sure the correct AWS region is set in both places.
3. Run the server application and then any number of clients.

# For more information or questions
- This steps in this file are condensed from the article found here: https://aws.amazon.com/blogs/gametech/how-to-set-up-player-authentication-with-amazon-cognito
- Please contact gametech@amazon.com for any comments or requests regarding this content.

# Visual Studio 2019 Note
Unfortunately the provided demo won’t compile directly in Visual Studio 2019, unless you have the Visual Studio 2017 platform toolset installed. The demo downloads the AWS SDK for C++ that’s available on NuGet, and the libraries are not compiled for the Visual Studio 2019 tooling. Refer to the instructions for building the AWS SDK for C++ in the developer guide here https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/setup.html. You’ll then need to configure your projects to find the include and library files in the SDK.

## License Summary

This sample code is made available under the Amazon Software License 1.0. See the LICENSE file.
