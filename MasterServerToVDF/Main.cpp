#include <string>
#include "ActiveSocket.h"       // Include header for active socket object definition
#include "PassiveSocket.h"
#pragma comment(lib,"ws2_32.lib")

#include <vector>
#include <algorithm>

#include <concurrent_vector.h>
#define safevector concurrency::concurrent_vector
#include <mutex>  
#include <sstream>
#include <Windows.h>

int MaxPackets = 180;
int MaxServers = 8000;
int MaxThreads = 100;

using namespace std;
bool printmoreinfo = false;
bool serverparser = true;
bool singlethread = false;


class SmallBinReader
{
public:
	std::vector<unsigned char> internaldata;
	bool ErrorFound = false;
	int Operations = 0; // for detect error

	SmallBinReader( const unsigned char * data, int size )
	{
		internaldata = std::vector<unsigned char>( data, data + size );
	}

	std::string ReadStr( )
	{
		if ( internaldata.empty( ) )
		{
			ErrorFound = true;
			return "";
		}
		Operations++;
		std::vector<unsigned char> outstr;

		while ( !internaldata.empty( ) && internaldata[ 0 ] != '\0' )
		{
			outstr.push_back( internaldata[ 0 ] );
			internaldata.erase( internaldata.begin( ) );
		}
		if ( !internaldata.empty( ) )
			internaldata.erase( internaldata.begin( ) );

		return std::string( &outstr[ 0 ], &outstr[ 0 ] + outstr.size( ) );
	}

	unsigned char ReadByte( )
	{
		if ( internaldata.empty( ) )
		{
			ErrorFound = true;
			return 0;
		}
		Operations++;
		unsigned char result = internaldata[ 0 ];
		internaldata.erase( internaldata.begin( ) );
		return result;
	}

	char ReadChar( )
	{
		if ( internaldata.empty( ) )
		{
			ErrorFound = true;
			return 0;
		}
		Operations++;

		char result = *( char* )&internaldata[ 0 ];
		internaldata.erase( internaldata.begin( ) );
		return result;
	}

	short ReadShort( )
	{
		if ( internaldata.empty( ) || internaldata.size( ) < 2 )
		{
			ErrorFound = true;
			return 0;
		}
		Operations++;

		short result = *( short* )&internaldata[ 0 ];
		internaldata.erase( internaldata.begin( ) );
		return result;
	}

	unsigned short ReadUShort( )
	{
		if ( internaldata.empty( ) || internaldata.size( ) < 2 )
		{
			ErrorFound = true;
			return 0;
		}
		Operations++;

		unsigned short result = *( unsigned short* )&internaldata[ 0 ];
		internaldata.erase( internaldata.begin( ) );
		return result;
	}

	int ReadInt( )
	{
		if ( internaldata.empty( ) || internaldata.size( ) < 4 )
		{
			ErrorFound = true;
			return 0;
		}
		Operations++;

		int result = *( int* )&internaldata[ 0 ];
		internaldata.erase( internaldata.begin( ) );
		internaldata.erase( internaldata.begin( ) );
		internaldata.erase( internaldata.begin( ) );
		internaldata.erase( internaldata.begin( ) );
		return result;
	}

	int ReadUInt( )
	{
		if ( internaldata.empty( ) || internaldata.size( ) < 4 )
		{
			ErrorFound = true;
			return 0;
		}
		Operations++;

		unsigned int result = *( unsigned int* )&internaldata[ 0 ];
		internaldata.erase( internaldata.begin( ) );
		internaldata.erase( internaldata.begin( ) );
		internaldata.erase( internaldata.begin( ) );
		internaldata.erase( internaldata.begin( ) );
		return result;
	}

};




void remove_duplicates( safevector<string> & vec )
{
	if ( printmoreinfo )
		printf( "%s\n", "Remove duplicates" );

	vector<string> tempunsafevector( vec.begin( ), vec.end( ) );

	std::sort( tempunsafevector.begin( ), tempunsafevector.end( ) );

	tempunsafevector.erase( std::unique( tempunsafevector.begin( ), tempunsafevector.end( ) ), tempunsafevector.end( ) );

	vec.clear( );
	for ( const string & s : tempunsafevector )
	{
		if ( !s.empty( ) )
			vec.push_back( s );
	}

	if ( printmoreinfo )
		printf( "%s\n", "Remove duplicates ok" );
}


std::vector<unsigned char> GenServerPacket( )
{
	vector<unsigned char> ServerReqHeader;

	// Packet Header
	ServerReqHeader.push_back( 0xFF );
	ServerReqHeader.push_back( 0xFF );
	ServerReqHeader.push_back( 0xFF );
	ServerReqHeader.push_back( 0xFF );
	// Strange string
	string MasterFilter = "TSource Engine Query";
	//string MasterFilter = "\\appid\\10";
	ServerReqHeader.insert( ServerReqHeader.end( ), MasterFilter.begin( ), MasterFilter.end( ) );
	ServerReqHeader.push_back( 0 );
	return ServerReqHeader;
}

std::vector<unsigned char> GenMasterServerPacket( string ip = "0.0.0.0:0", string filter = "" )
{
	vector<unsigned char> MasterHeader;

	// Packet ID
	MasterHeader.push_back( 0x31 );
	// Region 
	MasterHeader.push_back( 0xFF );
	// Start ip
	MasterHeader.insert( MasterHeader.end( ), ip.begin( ), ip.end( ) );
	MasterHeader.push_back( 0 );
	// Filter
	//string MasterFilter = "\\gamedir\\cstrike\\appid\\10";
	//string MasterFilter = "\\appid\\10";
	string MasterFilter = "\\gamedir\\cstrike";
	MasterHeader.insert( MasterHeader.end( ), MasterFilter.begin( ), MasterFilter.end( ) );
	MasterHeader.push_back( 0 );
	return MasterHeader;
}

//std::mutex mtx;


safevector<string> outips;
vector<string> servers;

void DumpServersToFiles( std::string filename )
{
	//mtx.lock( );
	ostringstream outserverlist;
	ostringstream outfavoriteslist;
	ostringstream outmasterserverlist;



	outmasterserverlist
		<< "\"MasterServers\"\n"
		<< "{\n"
		<< "\t\"hl1\"\n"
		<< "\t{\n";




	int serverid = 0;


	for ( const auto & s : servers )
	{
		outmasterserverlist << "\t\t\"" << serverid << "\"\n"
			<< "\t\t{\n"
			<< "\t\t\t\"addr\"\t\t\"" << s << "\"\n"
			<< "\t\t}\n\n";
		serverid++;
	}

	outmasterserverlist << "\t}\n\n"
		<< "}";


	outfavoriteslist
		<< "\"filters\"\n"
		<< "{\n"
		<< "\t\"favorites\"\n"
		<< "\t{\n";


	remove_duplicates( outips );


	serverid = 0;

	for ( const auto & s : outips )
	{
		outserverlist << s << "\n";
		outfavoriteslist
			<< "\t\t\"" << serverid << "\"\n"
			<< "\t\t{\n"
			<< "\t\t\t\"name\"\t\"" << serverid << "\"\n"
			<< "\t\t\t\"address\"\t\"" << s << "\"\n"
			<< "\t\t\t\"lastplayed\"\t\"0\"\n"
			<< "\t\t\t\"appID\"\t\"10\"\n"
			<< "\t\t\t\"gamedir\"\t\"cstrike\"\n"
			<< "\t\t}\n\n";
		serverid++;
	}

	outfavoriteslist
		<< "\t}\n\n"
		<< "\t\"history\"\n"
		<< "\t{\n\n"
		<< "\t}\n\n"
		<< "}";



	FILE * f;
	fopen_s( &f, filename.c_str( ), "w" );
	if ( f )
	{
		fwrite( outserverlist.str( ).data( ), outserverlist.str( ).size( ), 1, f );
		fclose( f );
	}
	f = NULL;
	fopen_s( &f, "rev_ServerBrowser.vdf", "w" );
	if ( f )
	{
		fwrite( outfavoriteslist.str( ).data( ), outfavoriteslist.str( ).size( ), 1, f );
		fclose( f );
	}

	f = NULL;
	fopen_s( &f, "rev_MasterServers.vdf", "w" );
	if ( f )
	{
		fwrite( outmasterserverlist.str( ).data( ), outmasterserverlist.str( ).size( ), 1, f );
		fclose( f );
	}

	//mtx.unlock( );
}

void ParseAddress( std::string addr, std::string & outaddr, int & outport )
{
	const char * xaddr = "";
	const char * port = "";

	int id = 0;

	for ( auto & c : addr )
	{
		if ( c == ':' )
		{
			c = '\0';
			xaddr = addr.c_str( );
			port = ( const char* )( addr.data( ) + ( id + 1 ) );
		}

		id++;
	}

	outport = atoi( port );
	outaddr = xaddr;
}

int connectstarted = 0;


safevector<string> OutServerInfo;

safevector<string> OutServerCleanIp;

struct IPvec
{
	unsigned char ip1;
	unsigned char ip2;
	unsigned char ip3;
	unsigned char ip4;
	unsigned short port;
};

safevector<IPvec> MasterServerIpList;


IPvec GetIPVecFromString( const std::string & minstr )
{
	char  ip_port[ 512 ];

	sprintf_s( ip_port, "%s", minstr.c_str( ) );
	const char * ip1 = ip_port;
	const char * ip2 = "2";
	const char * ip3 = "3";
	const char * ip4 = "4";
	const char * port = "5";


	int id = 0;
	int i = 0;

	for ( int i = 1; i < minstr.length( ) - 1; i++ )
	{
		char & s = ip_port[ i ];
		if ( s == '.' )
		{
			id++;
			s = '\0';

			if ( id == 1 )
			{
				ip2 = &ip_port[ i + 1 ];
			}
			if ( id == 2 )
			{
				ip3 = &ip_port[ i + 1 ];
			}
			if ( id == 3 )
			{
				ip4 = &ip_port[ i + 1 ];
			}
		}
		else if ( s == ':' )
		{
			s = '\0';
			port = &ip_port[ i + 1 ];
			break;
		}
	}

	IPvec tmpIPvec = IPvec( );
	tmpIPvec.ip1 = atoi( ip1 );
	tmpIPvec.ip2 = atoi( ip2 );
	tmpIPvec.ip3 = atoi( ip3 );
	tmpIPvec.ip4 = atoi( ip4 );
	tmpIPvec.port = atoi( port );
	return tmpIPvec;
}




void DumpServersInfoToFile( )
{

	ostringstream serverinfolist;
	for ( const auto & s : OutServerInfo )
	{
		serverinfolist << s << "\n";
	}
	FILE * f;
	fopen_s( &f, "serverinfolist.txt", "w" );
	if ( f )
	{
		fwrite( serverinfolist.str( ).data( ), serverinfolist.str( ).size( ), 1, f );
		fclose( f );
	}



}

DWORD LastUpdate = GetTickCount( );

double SERVERPARSERTHREADPROGRESS = 0.0;


struct IpBadStr
{
	std::string ip;
	int badcount;
};


safevector<IpBadStr> IpBadStrList;


void AddBadIp( std::string ip )
{
	bool foundip = false;

	for ( auto & s : IpBadStrList )
	{
		if ( s.ip == ip )
		{
			auto tmps = s;
			tmps.badcount++;
			foundip = true;
			s = tmps;
			break;
		}
	}
	if ( !foundip )
	{
		IpBadStr tmpIpBadStr = IpBadStr( );
		tmpIpBadStr.ip = ip;
		tmpIpBadStr.badcount = 1;
		IpBadStrList.push_back( tmpIpBadStr );
	}
}

int GetBadIP( std::string ip )
{
	for ( const auto & s : IpBadStrList )
	{
		if ( s.ip == ip )
		{
			return s.badcount;
		}
	}

	return 0;
}

DWORD WINAPI SERVERPARSERTHREAD( char * val )
{
	LastUpdate = GetTickCount( );


	std::string curserver = val;
	bool ServerChecked = false;
	int trycount = 5;

	delete[ ] val;


	int PortNo = 27010;
	std::string curaddr;
	ParseAddress( curserver, curaddr, PortNo );

	if ( GetBadIP( curaddr ) > 6 )
	{
		connectstarted--;
		SERVERPARSERTHREADPROGRESS += 1.0;
		return 0;
	}


	
	CActiveSocket socket( CSimpleSocket::SocketTypeUdp );       // Instantiate active socket object (defaults to TCP).

	socket.Initialize( );
	socket.SetConnectTimeout( 2500 );



	if ( printmoreinfo )
		std::printf( "trying to connect to \"%s\":\"%d\" ..\n", curaddr.c_str( ), PortNo );

	if ( printmoreinfo )
		printf( "Tried %i....\n", ( 5 - trycount ) );

	if ( socket.Open( curaddr.c_str( ), ( uint16 )PortNo ) )
	{

		socket.SetReceiveTimeout( 1600 );
		socket.SetSendTimeout( 1600 );


		auto ServerReqHeader = GenServerPacket( );

		for ( auto c : ServerReqHeader )
		{
			if ( printmoreinfo )
				std::printf( "%02X", c );
		}

		if ( printmoreinfo )
			std::printf( "\n%s\n", "End" );

		bool fixederror = false;

		if ( socket.Send( ( const uint8 * )ServerReqHeader.data( ), ServerReqHeader.size( ) ) )
		{
		retrygetnormalheader:
			if ( socket.Receive( 8112 ) > 0 )
			{
				int bytesreceived = socket.GetBytesReceived( );
				if ( bytesreceived >= 15 )
				{
					if ( printmoreinfo )
						std::printf( "SERVER GET RESPONSE. Size:%d bytes\n", bytesreceived );

					SmallBinReader * tmpSmallBinReader = new SmallBinReader( socket.GetData( ), bytesreceived );

					int preheader = tmpSmallBinReader->ReadUInt( );
					unsigned char header = tmpSmallBinReader->ReadByte( );

					if ( preheader == 0xFFFFFFFF )
					{
						if ( header == 0x44 && !fixederror )
						{
							fixederror = true;
							std::printf( "ERROR GOT PLAYERLIST BUT WAIT SERVERINFO!\n" );

							std::printf( "Retry for got normal header..." );

							goto retrygetnormalheader;
						}

						if ( header == 0x6D )
						{

							if ( printmoreinfo )
								std::printf( "HEADER OKAY\n" );

							std::string ServerAddr = tmpSmallBinReader->ReadStr( );
							std::string ServerName = tmpSmallBinReader->ReadStr( );
							std::string ServerMap = tmpSmallBinReader->ReadStr( );
							std::string ServerFolder = tmpSmallBinReader->ReadStr( );
							std::string ServerGame = tmpSmallBinReader->ReadStr( );

							unsigned char players = tmpSmallBinReader->ReadByte( );
							unsigned char maxplayers = tmpSmallBinReader->ReadByte( );
							unsigned char protocol = tmpSmallBinReader->ReadByte( );
							char servertype = tmpSmallBinReader->ReadChar( );
							char serveros = tmpSmallBinReader->ReadChar( );
							unsigned char needpassword = tmpSmallBinReader->ReadByte( );
							unsigned char serverwithmod = tmpSmallBinReader->ReadByte( );
							std::string ModWebsite = "";
							std::string ModDownloadLink = "";
							int ModVersion = 0;
							int ModSize = 0;
							int ModType = 0;
							int ModDll = 0;


							if ( serverwithmod )
							{
								ModWebsite = tmpSmallBinReader->ReadStr( );
								ModDownloadLink = tmpSmallBinReader->ReadStr( );
								tmpSmallBinReader->ReadByte( );
								ModVersion = tmpSmallBinReader->ReadInt( );
								ModSize = tmpSmallBinReader->ReadInt( );
								ModType = tmpSmallBinReader->ReadByte( );
								ModDll = tmpSmallBinReader->ReadByte( );
							}
							unsigned char withantihack = tmpSmallBinReader->ReadByte( );
							unsigned char botcount = tmpSmallBinReader->ReadByte( );

							players -= botcount;


							if ( !tmpSmallBinReader->ErrorFound || serverwithmod )
							{
								char servdata[ 32 ];
								sprintf_s( servdata, "%d", ( unsigned int )protocol );

								std::string resultserverstring = curserver + ":" + servdata + ":" + ServerName;
								OutServerInfo.push_back( resultserverstring );
								OutServerCleanIp.push_back( curserver );
								MasterServerIpList.push_back( GetIPVecFromString( curserver ) );
							}
							else
							{
								std::printf( "ERROR FOUND BAD HEADER v1. Operation id: %i\n", tmpSmallBinReader->Operations );
							}
						}
						else if ( header == 0x49 )
						{
							unsigned char protocol = tmpSmallBinReader->ReadByte( );
							std::string ServerName = tmpSmallBinReader->ReadStr( );
							std::string ServerMap = tmpSmallBinReader->ReadStr( );
							std::string ServerFolder = tmpSmallBinReader->ReadStr( );
							std::string ServerGame = tmpSmallBinReader->ReadStr( );
							unsigned short appid = tmpSmallBinReader->ReadUShort( );
							unsigned char players = tmpSmallBinReader->ReadByte( );
							unsigned char maxplayers = tmpSmallBinReader->ReadByte( );
							unsigned char botcount = tmpSmallBinReader->ReadByte( );
							char servertype = tmpSmallBinReader->ReadChar( );
							char serveros = tmpSmallBinReader->ReadChar( );
							unsigned char needpassword = tmpSmallBinReader->ReadByte( );
							unsigned char withantihack = tmpSmallBinReader->ReadByte( );
							std::string ServerVersion = tmpSmallBinReader->ReadStr( );
							//next extra data


							if ( !tmpSmallBinReader->ErrorFound )
							{
								char servdata[ 32 ];
								sprintf_s( servdata, "%d", ( unsigned int )protocol );

								std::string resultserverstring = "+" + curserver + ":" + servdata + ":" + ServerName;
								OutServerInfo.push_back( resultserverstring );
								OutServerCleanIp.push_back( curserver );
								MasterServerIpList.push_back( GetIPVecFromString( curserver ) );
							}
							else
							{
								std::printf( "ERROR FOUND BAD HEADER v2 . Operation id: %i\n", tmpSmallBinReader->Operations );
							}
						}
						else
						{
							std::printf( "ERROR FOUND UNKNOWN HEADER %X \n", ( unsigned int )header );
						}
					}
					else
					{
						std::printf( "ERROR FOUND UNKNOWN HEADER & PREHEADER RESPONSE %X -> %X \n", preheader, ( unsigned int )header );
					}

					delete tmpSmallBinReader;
				}
			}
			else
			{
				AddBadIp( curaddr );
			}
		}
		else
		{
			AddBadIp( curaddr );
		}
	}
	else
	{
		AddBadIp( curaddr );
	}
	socket.Close( );
	connectstarted--;
	SERVERPARSERTHREADPROGRESS += 1.0;

	return 0;
}

double MASTERSERVERPARSERTHREADPROGRESS = 0.0;

DWORD WINAPI MASTERSERVERPARSERTHREAD( char * val )
{
	std::string curserver = val;
	bool ServerChecked = false;
	int trycount = 5;

	delete[ ] val;

	bool firstipskipped = false;

	string StartedAddress = "0.0.0.0:0";
	string StartedAddress2 = "1.0.0.0:0";

	int PortNo = 27010;
	std::string curaddr;
	ParseAddress( curserver, curaddr, PortNo );

	if ( printmoreinfo )
		std::printf( "trying to connect to \"%s\":\"%d\" ..\n", curaddr.c_str( ), PortNo );


	CActiveSocket socket( CSimpleSocket::SocketTypeUdp );       // Instantiate active socket object (defaults to TCP).

	socket.Initialize( );



	//--------------------------------------------------------------------------
	// Create a connection to the time server so that data can be sent
	// and received.
	//--------------------------------------------------------------------------

	socket.SetConnectTimeout( 3500 );


	if ( printmoreinfo )
		printf( "Tried %i....\n", ( 5 - trycount ) );

	if ( socket.Open( curaddr.c_str( ), ( uint16 )PortNo ) )
	{

		socket.SetReceiveTimeout( 3500 );
		socket.SetSendTimeout( 3500 );


		//----------------------------------------------------------------------
		// Send a requtest the server requesting the current time.
		//----------------------------------------------------------------------



		auto MasterHeader = GenMasterServerPacket( StartedAddress );

		for ( auto c : MasterHeader )
		{
			if ( printmoreinfo )
				std::printf( "%02X", c );
		}

		if ( printmoreinfo )
			std::printf( "\n%s\n", "End" );
		int packetssend = 0;
		int serversrecv = 0;
		int oldbytesreceived = 0;


		while ( socket.Send( ( const uint8 * )MasterHeader.data( ), MasterHeader.size( ) ) && StartedAddress2 != StartedAddress )
		{
			

			StartedAddress2 = StartedAddress;

			if ( !ServerChecked )
			{
				ServerChecked = true;
				servers.push_back( curserver );
			}

			//----------------------------------------------------------------------
			// Receive response from the server.
			//----------------------------------------------------------------------


			if ( socket.Receive( 8192 ) > 0 )
			{
				packetssend++;
				if ( packetssend > MaxPackets )
				{
					if ( printmoreinfo )
						std::printf( "Packet limit. End..." );
					socket.Close( );
					connectstarted--;
					MASTERSERVERPARSERTHREADPROGRESS += 1.0;
					return 0;
				}

				int bytesreceived = socket.GetBytesReceived( );
				if ( bytesreceived == -1 )
				{
					if ( printmoreinfo )
						std::printf( "Unknown fatal error. End..." );
					socket.Close( );
					connectstarted--;
					MASTERSERVERPARSERTHREADPROGRESS += 1.0;
					return 0;
				}

				if ( printmoreinfo )
					std::printf( "Bytes received : %i (~%i ip)\n", bytesreceived, bytesreceived / 6 );


				serversrecv += bytesreceived / 6;
				if ( serversrecv > MaxServers )
				{
					if ( printmoreinfo )
						std::printf( "Server limit. End..." );
					socket.Close( );
					connectstarted--;
					MASTERSERVERPARSERTHREADPROGRESS += 1.0;
					return 0;
				}


				string LastIp = "";

				for ( int i = 0; i < bytesreceived; i += 6 )
				{
					if ( !firstipskipped )
					{
						firstipskipped = true;
						continue;
					}
					byte a1 = socket.GetData( )[ i ];
					byte a2 = socket.GetData( )[ i + 1 ];
					byte a3 = socket.GetData( )[ i + 2 ];
					byte a4 = socket.GetData( )[ i + 3 ];


					byte portbyte[ 2 ]{ socket.GetData( )[ i + 5 ],socket.GetData( )[ i + 4 ] };


					unsigned short port = *( unsigned short* )&portbyte;

					char outip[ 32 ];
					sprintf_s( outip, "%i.%i.%i.%i:%i", a1, a2, a3, a4, port );

					LastIp = outip;
					StartedAddress = LastIp;

					if ( a1 == a2 && a2 == a3 && a3 == a4 && a4 == 0 )
					{
						oldbytesreceived = -2;

						if ( printmoreinfo )
							std::printf( "NULL IP. End..." );
						socket.Close( );
						connectstarted--;
						MASTERSERVERPARSERTHREADPROGRESS += 1.0;
						return 0;
					}
					if ( port == 0 )
					{
						oldbytesreceived = -2;

						if ( printmoreinfo )
							std::printf( "NULL POR FOUND!. End..." );
						socket.Close( );
						connectstarted--;
						MASTERSERVERPARSERTHREADPROGRESS += 1.0;
						return 0;
					}

					if ( a1 == a2 && a2 == a3 && a3 == a4 && a4 == 1 )
					{
						continue;
					}
					if ( a1 == a2 && a2 == a3 && a3 == a4 && a4 == 255 )
					{
						continue;
					}

					outips.push_back( outip );
				}

				//std::printf( "Bytes parsed\n" );
				if ( oldbytesreceived != bytesreceived && oldbytesreceived != 0 )
				{
					if ( printmoreinfo )
						std::printf( "Bytes parsed\n" );
					socket.Close( );
					connectstarted--;
					MASTERSERVERPARSERTHREADPROGRESS += 1.0;
					return 0;
				}

				oldbytesreceived = bytesreceived;


				if ( printmoreinfo )
					std::printf( "Recv new packet with ip: %s (all ip : %u)\n", LastIp.c_str( ), outips.size( ) );


				MasterHeader = GenMasterServerPacket( StartedAddress );

				//if ( socket.Send( ( const uint8 * )MasterHeader.data( ), MasterHeader.size( ) ) )
				//{

				//}
				//else
				//{
				//	if ( printmoreinfo )
				//		std::printf( "%s\n", "ERRORRRRR" );
				//	Sleep( 1000 );
				//	break;
				//}


				//std::printf( "Bytes parsed 3\n" );
				if ( printmoreinfo )
					std::printf( "\nEnd received : %u ip\n", outips.size( ) );
			}
			else
			{
				if ( printmoreinfo )
					std::printf( "\nEnd received : %u ip WITH ERROR\n", outips.size( ) );
				break;
			}


		}
	}
	else
	{
		//std::printf( "%s\n", "ERRORRRRR" );
		Sleep( 1000 );
	}
	socket.Close( );

	connectstarted--;
	MASTERSERVERPARSERTHREADPROGRESS += 1.0;
	return 0;
}

std::vector<std::vector<unsigned char>> BuildMasterPacketsOut( IPvec startedip )
{
	unsigned char buf[ 6 ];
	memset( &buf[ 0 ], 255, 6 );


	std::vector<std::vector<unsigned char>> outPackets;

	std::vector<unsigned char> outPacket;



	for ( int i = 0; i < 6; i++ )
	{
		outPacket.push_back( buf[ i ] );
	}

	for ( unsigned int i = 0; i < MasterServerIpList.size( ); i++ )
	{
		memcpy( buf, &MasterServerIpList[ i ], 6 );
		for ( int i = 0; i < 6; i++ )
		{
			outPacket.push_back( buf[ i ] );
		}

		if ( i % 200 )
		{
			outPackets.push_back( outPacket );
			outPacket.clear( );
			memset( &buf[ 0 ], 255, 6 );
			for ( int i = 0; i < 6; i++ )
			{
				outPacket.push_back( buf[ i ] );
			}
		}

	}

	memcpy( buf, &startedip, 6 );
	for ( int i = 0; i < 6; i++ )
	{
		outPacket.push_back( buf[ i ] );
	}

	outPackets.push_back( outPacket );
	return outPackets;
}


DWORD WINAPI MASTERSERVERTHREAD( LPVOID )
{

	/*
	auto ipoutservers = BuildMasterPacketsOut( GetIPVecFromString( startip ) );

	for ( auto s : ipoutservers )
	{
		send;
	}*/


	return 0;
}


void PrintUsage( )
{
	std::printf( "Usage: MasterServerToVDF.exe [options] /ms (/masterservers) [list]\n" );
	std::printf( "/v (/versbose) - show more output info\n" );
	std::printf( "/c (/check) - enable check out servers\n" );
	std::printf( "/conn (/connections) [num] - set maximum threads\n" );
	std::printf( "/servers [num] - set maximum servers per masterserver\n" );
	std::printf( "/packets [num] - set maximum packets per masterserver\n" );
}

int main( int argc, char **argv )
{
	vector<string> servers;
	bool MasterServersNeed = false;

	for ( int i = 1; i < argc; ++i ) {
		std::string arg = argv[ i ];
		if ( ( arg == "/h" ) || ( arg == "/?" ) || ( arg == "/help" ) ) {
			PrintUsage( );
			return 0;
		}
		else if ( ( arg == "/v" ) || ( arg == "/verbose" ) ) {
			MasterServersNeed = false;
			std::printf( "Show more info enabled\n" );
			printmoreinfo = true;
		}
		else if ( ( arg == "/conn" ) || ( arg == "/connections" ) ) {
			MasterServersNeed = false;
			if ( i + 1 < argc ) {
				++i;
				std::printf( "Set maximum connection to %s\n", argv[ i ] );
				MaxThreads = atoi( argv[ i ] );
			}
			else
			{
				PrintUsage( );
				return 0;
			}
		}
		else if ( ( arg == "/servers" ) ) {
			MasterServersNeed = false;
			if ( i + 1 < argc ) {
				++i;
				std::printf( "Set maximum servers parsed from masterserver to %s\n", argv[ i ] );
				MaxServers = atoi( argv[ i ] );
			}
			else
			{
				PrintUsage( );
				return 0;
			}
		}
		else if ( ( arg == "/test" ) ) {
			MasterServersNeed = false;
			CreateThread( 0, 0, MASTERSERVERTHREAD, 0, 0, 0 );
			Sleep( 1000 );
			servers.push_back( "127.0.0.1:27010" );
		}
		else if ( ( arg == "/packets" ) ) {
			MasterServersNeed = false;
			if ( i + 1 < argc ) {
				++i;
				std::printf( "Set maximum packet from masterserver to %s\n", argv[ i ] );
				MaxPackets = atoi( argv[ i ] );
			}
			else
			{
				PrintUsage( );
				return 0;
			}
		}
		else if ( ( arg == "/si" ) || ( arg == "/serverinfo" ) ) {
			std::printf( "After receive servers from masterservers, parse info from all servers to file.\n" );
			serverparser = true;
			MasterServersNeed = false;
		}
		else if ( ( arg == "/s" ) || ( arg == "/singlethread" ) ) {
			std::printf( "Multithreading disabled\n" );
			singlethread = true;
			MasterServersNeed = false;
		}
		else if ( ( arg == "/ms" ) || ( arg == "/masterservers" ) ) {
			if ( i + 1 < argc ) {
				MasterServersNeed = true;
			}
			else {
				PrintUsage( );
				return 0;
			}
		}
		else if ( arg.length( ) > 8 && MasterServersNeed ) {
			servers.push_back( arg );
		}
	}

	if ( servers.empty( ) )
	{
		servers.push_back( "ms1.amxboost.ru:27010" );
	}

	for ( const auto & s : servers )
	{
		std::printf( "%s\n", s.c_str( ) );
	}

	//--------------------------------------------------------------------------
	// Initialize our socket object 
	//--------------------------------------------------------------------------




	for ( const auto & curserver : servers )
	{
		if ( singlethread )
		{
			char * c_server = new char[ 256 ];
			sprintf_s( c_server, 256, "%s", curserver.c_str( ) );
			MASTERSERVERPARSERTHREAD( c_server );
		}
		else
		{
			char * c_server = new char[ 256 ];
			sprintf_s( c_server, 256, "%s", curserver.c_str( ) );
			CloseHandle( CreateThread( 0, 0, ( LPTHREAD_START_ROUTINE )MASTERSERVERPARSERTHREAD, c_server, 0, 0 ) );
			connectstarted++;

			while ( connectstarted > MaxThreads )
			{
				if ( !printmoreinfo )
					Sleep( 15 );
				else
					Sleep( 500 );

				int percent = ( int )( MASTERSERVERPARSERTHREADPROGRESS / servers.size( ) * 100.0 );

				printf( "%s[ Remaining threads: %i Servers found: %u     %u%%  ]     ", printmoreinfo ? "\n" : "\r", connectstarted, outips.size( ), percent );
			}

		}
	}

	printf( "\nStart parsing %u servers...\n\n", servers.size( ) );

	while ( connectstarted > 0 )
	{
		if ( !printmoreinfo )
			Sleep( 15 );
		else
			Sleep( 500 );
		int percent = ( int )( MASTERSERVERPARSERTHREADPROGRESS / ( double )servers.size( ) * 100.0 );

		printf( "%s[ Remaining threads: %i Servers found: %u     %u%%  ]     ", printmoreinfo ? "\n" : "\r", connectstarted, outips.size( ), percent );
	}

	printf( "\n%s%u\n", "OK. Servers parsed:", outips.size( ) );

	printf( "Removing duplicates and generate output files....\n" );

	DumpServersToFiles( "nonfilterediplist.txt" );

	printf( "\n\n%s%u%s\n", "OK. Found:", outips.size( ), " unique servers." );



	// Server check pass 

	if ( serverparser )
	{
		auto founserverlist = outips;

		for ( const auto & curserver : founserverlist )
		{
			if ( singlethread )
			{
				char * c_server = new char[ 256 ];
				sprintf_s( c_server, 256, "%s", curserver.c_str( ) );
				SERVERPARSERTHREAD( c_server );
			}
			else
			{
				char * c_server = new char[ 256 ];
				sprintf_s( c_server, 256, "%s", curserver.c_str( ) );
				CloseHandle( CreateThread( 0, 0, ( LPTHREAD_START_ROUTINE )SERVERPARSERTHREAD, c_server, 0, 0 ) );
				connectstarted++;

				while ( connectstarted > MaxThreads )
				{
					if ( !printmoreinfo )
						Sleep( 75 );
					else
						Sleep( 500 );
					int percent = ( int )( SERVERPARSERTHREADPROGRESS / founserverlist.size( ) * 100.0 );


					printf( "%s[ Remaining threads: %i Servers parsed: %u     %u%%  ]     ", printmoreinfo ? "\n" : "\r", connectstarted, OutServerInfo.size( ), percent );
				}

			}
		}
	}

	while ( connectstarted > 0 )
	{
		if ( !printmoreinfo )
			Sleep( 75 );
		else
			Sleep( 500 );
		int percent = ( int )( SERVERPARSERTHREADPROGRESS / ( double )outips.size( ) * 100.0 );


		printf( "%s[ Remaining threads: %i Servers parsed: %u     %u%%  ]     ", printmoreinfo ? "\n" : "\r", connectstarted, OutServerInfo.size( ), percent );

		if ( GetTickCount( ) - LastUpdate > 15000 )
		{
			printf( "Error! Timeout./n" );
			break;
		}

	}

	DumpServersInfoToFile( );

	printf( "\n\n%s%u%s\n", "OK. Parsed:", OutServerInfo.size( ), " servers info." );

	outips = OutServerCleanIp;

	DumpServersToFiles( "serverlist.txt" );

	printf( "\n\n%s%u%s\n", "OK. Found:", outips.size( ), " unique servers." );




	system( "pause" );

	return 1;
}