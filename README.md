Server used by Juggluco to exchange connection data between two instance of Juggluco so that they can communicate via ICE.  
In left middle menu-> Mirror when in Add connection ICE is set or when with AutoQR, Internet is selected. 
Juggluco assumes the server listens at port 6789.
The hostname (in double quotes) of the server should be set in Common/src/main/cpp/net/ICE/jugglucoconnect.h
For example:
"servername.org"

