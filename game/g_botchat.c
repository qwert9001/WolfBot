// Copyright (C) 2003 Torben Könke. Licensed under GPL, see LICENSE.
//
// g_botchat.c -- AI chat stuff

#include "g_local.h"

#define MAX_BOTCHAT_FILE	64000
#define MAX_LINE_LENGTH		80
#define MAX_CHAT_SENTENCES	100
#define MAX_CHAT_EVENTS		5
#define MAX_CHAT_WORDLISTS	25

typedef enum
{
	BOTH_CHAT,
	AXIS_CHAT,
	ALLIES_CHAT

} enum_botchat_t;

typedef struct
{
	char	szEventName[32];
	int		iNumAttackers;
	int		iNumDefenders;
	char	szAttackers[25][MAX_LINE_LENGTH];
	char	szDefenders[25][MAX_LINE_LENGTH];

} eventChat_t; 

typedef struct
{
	char szIdentifier[32];
	int iNumWords;
	char **pWords;

} wordList_t;

// General Chat
char szGeneralChat[3][MAX_CHAT_SENTENCES][MAX_LINE_LENGTH];
int	iGeneralChatCount[3];

// Dead Chat
char szDeadChat[3][MAX_CHAT_SENTENCES][MAX_LINE_LENGTH];
int iDeadChatCount[3];

// Event Chat
eventChat_t eventChats[MAX_CHAT_EVENTS];
int iNumEventChats;

// Word Chat

// Word Lists
wordList_t wordLists[MAX_CHAT_WORDLISTS];
int iNumWordLists;

BOOL bChatFileLoaded = FALSE;

//
// Parses the botchat.cfg file
//

void Bot_LoadChatFile( void )
{
	fileHandle_t fHandle;
	int iLength;
	char szBuffer[MAX_BOTCHAT_FILE];
	char *pToken, *pFile;
	BOOL bAborted;

	iLength = trap_FS_FOpenFile( wb_chatFile.string, &fHandle, FS_READ );

	if( !fHandle )
	{
		G_Printf("Warning could not open botchat.cfg\n");
		return;
	}

	if( iLength > MAX_BOTCHAT_FILE )
	{
		G_Printf("Chat file is too big, make it smaller.\n");
		return;
	}

	trap_FS_Read( szBuffer, iLength, fHandle );
	pFile = szBuffer;

	bAborted = FALSE;

	memset( iGeneralChatCount, 0, sizeof(int) * 3 );
	memset( iDeadChatCount, 0, sizeof(int) * 3 );
	memset( eventChats, 0, sizeof(eventChat_t) * MAX_CHAT_EVENTS );
	memset( wordLists, 0, sizeof(wordList_t) * MAX_CHAT_WORDLISTS );
	iNumEventChats = 0;
	iNumWordLists = 0;

	while(1)
	{
		pToken = COM_ParseExt( &pFile, TRUE );

		if( !pToken[0] )
			break;

		// parse the general chats
		if( !Q_stricmp( pToken, "GeneralChat" ) )
		{
			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			if( strcmp( pToken, "{" ) )
			{
				G_Printf("Parse Error : Missing { in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			while(1)
			{
				pToken = COM_ParseExt( &pFile, TRUE );

				if( !pToken[0] )
				{
					G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				// parse general axis chat
				if( !Q_stricmp( pToken, "axis" ) )
				{
					if( g_debugBots.integer )
						G_Printf("Parsing general axis chat\n");

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n", pToken);
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						// read line into array
						Q_strncpyz( szGeneralChat[AXIS_CHAT][ iGeneralChatCount[AXIS_CHAT]++ ], pToken, MAX_LINE_LENGTH );

						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
							break;

						if( !strcmp( pToken, "," ) )
						{
							continue;
						}
						else if( !strcmp( pToken, "}" ) )
						{
							// done parsing general axis chat
							break;
						}
						else
						{
							G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
							bAborted = TRUE;
							break;
						}
					}
				}
				else if( !Q_stricmp( pToken, "allies" ) )
				{
					if( g_debugBots.integer )
						G_Printf("Parsing general allies chat\n");

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n", pToken);
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						// read line into array
						Q_strncpyz( szGeneralChat[ALLIES_CHAT][ iGeneralChatCount[ALLIES_CHAT]++ ], pToken, MAX_LINE_LENGTH );

						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
							break;

						if( !strcmp( pToken, "," ) )
						{
							continue;
						}
						else if( !strcmp( pToken, "}" ) )
						{
							// done parsing general allies chat
							break;
						}
						else
						{
							G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
							bAborted = TRUE;
							break;
						}
					}
				}
				else if( !Q_stricmp( pToken, "both" ) )
				{
					if( g_debugBots.integer )
						G_Printf("Parsing general chat\n");

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n", pToken);
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						// read line into array
						Q_strncpyz( szGeneralChat[BOTH_CHAT][ iGeneralChatCount[BOTH_CHAT]++ ], pToken, MAX_LINE_LENGTH );

						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
							break;

						if( !strcmp( pToken, "," ) )
						{
							continue;
						}
						else if( !strcmp( pToken, "}" ) )
						{
							// done parsing general chat
							break;
						}
						else
						{
							G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
							bAborted = TRUE;
							break;
						}
					}
				}
				else if( !Q_stricmp( pToken, "}" ) )
				{
					// done parsing general chat section
					break;
				}
				else
				{
					G_Printf("Warning : Unknown keyword in botchat.cfg : %s\n", pToken);
					break;
				}
			}
		}
		else if( !Q_stricmp( pToken, "DeadChat" ) )
		{
			// parse the dead chat
			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			if( strcmp( pToken, "{" ) )
			{
				G_Printf("Parse Error : Missing { in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			while(1)
			{
				pToken = COM_ParseExt( &pFile, TRUE );

				if( !pToken[0] )
				{
					G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				// parse dead axis chat
				if( !Q_stricmp( pToken, "axis" ) )
				{
					if( g_debugBots.integer )
						G_Printf("Parsing dead axis chat\n");

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n", pToken);
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						// read line into array
						Q_strncpyz( szDeadChat[AXIS_CHAT][ iDeadChatCount[AXIS_CHAT]++ ], pToken, MAX_LINE_LENGTH );

						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
							break;

						if( !strcmp( pToken, "," ) )
						{
							continue;
						}
						else if( !strcmp( pToken, "}" ) )
						{
							// done parsing dead axis chat
							break;
						}
						else
						{
							G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
							bAborted = TRUE;
							break;
						}
					}
				}
				else if( !Q_stricmp( pToken, "allies" ) )
				{
					if( g_debugBots.integer )
						G_Printf("Parsing dead allies chat\n");

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n", pToken);
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						// read line into array
						Q_strncpyz( szDeadChat[ALLIES_CHAT][ iDeadChatCount[ALLIES_CHAT]++ ], pToken, MAX_LINE_LENGTH );

						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
							break;

						if( !strcmp( pToken, "," ) )
						{
							continue;
						}
						else if( !strcmp( pToken, "}" ) )
						{
							// done parsing dead allies chat
							break;
						}
						else
						{
							G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
							bAborted = TRUE;
							break;
						}
					}
				}
				else if( !Q_stricmp( pToken, "both" ) )
				{
					if( g_debugBots.integer )
						G_Printf("Parsing dead chat\n");

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n", pToken);
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						// read line into array
						Q_strncpyz( szDeadChat[BOTH_CHAT][ iDeadChatCount[BOTH_CHAT]++ ], pToken, MAX_LINE_LENGTH );

						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
							break;

						if( !strcmp( pToken, "," ) )
						{
							continue;
						}
						else if( !strcmp( pToken, "}" ) )
						{
							// done parsing dead chat
							break;
						}
						else
						{
							G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
							bAborted = TRUE;
							break;
						}
					}
				}
				else if( !Q_stricmp( pToken, "}" ) )
				{
					// done parsing dead chat section
					break;
				}
				else
				{
					G_Printf("Warning : Unknown keyword in botchat.cfg : %s\n", pToken);
					break;
				}
			}
		}
		else if( !Q_stricmp( pToken, "EventChat" ) )
		{
			// parse the event chat
			if( g_debugBots.integer )
				G_Printf("Parsing the Event Chat\n");

			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			if( strcmp( pToken,"{" ) )
			{
				G_Printf("Parse Error : Missing { in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			while(1)
			{
				pToken = COM_ParseExt( &pFile, TRUE );

				if( !pToken[0] )
				{
					G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				if( !Q_stricmp( pToken, "onEvent" ) )
				{
					// parse an event chat
					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( iNumEventChats >= MAX_CHAT_EVENTS )
					{
						G_Printf("Parse Error : iNumChatEvents >= MAX_EVENT_CHATS\n");
						bAborted = TRUE;
						break;
					}

					strcpy( eventChats[ iNumEventChats ].szEventName, pToken );

					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( strcmp( pToken, "{" ) )
					{
						G_Printf("Parse Error : Missing { in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					while(1)
					{
						pToken = COM_ParseExt( &pFile, TRUE );

						if( !pToken[0] )
						{
							G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
							bAborted = TRUE;
							break;
						}

						if( !strcmp( pToken, "}" ) )
						{
							// done with this event assume everything went ok
							iNumEventChats++;

							break;
						}

						// parse attackers chat
						if( !Q_stricmp( pToken, "attackers" ) )
						{
							pToken = COM_ParseExt( &pFile, TRUE );

							if( !pToken[0] )
							{
								G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
								bAborted = TRUE;
								break;
							}

							if( strcmp( pToken, "{" ) )
							{
								G_Printf("Parse Error : Missing { in botchat.cfg\n");
								bAborted = TRUE;
								break;
							}

							while(1)
							{
								pToken = COM_ParseExt( &pFile, TRUE );

								if( !pToken[0] )
								{
									G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
									bAborted = TRUE;
									break;
								}

								// read the attackers sentence
								strcpy( eventChats[ iNumEventChats ].szAttackers[ eventChats[ iNumEventChats ].iNumAttackers++ ], pToken );

								pToken = COM_ParseExt( &pFile, TRUE );

								if( !pToken[0] )
								{
									G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
									bAborted = TRUE;
									break;
								}

								if( !strcmp( pToken, "," ) )
								{
									continue;
								}
								else if( !strcmp( pToken, "}" ) )
								{
										// done parsing attackers chat stuff
										break;
								}
								else
								{
									G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
									bAborted = TRUE;
									break;
								}
							}
						}
						else if( !Q_stricmp( pToken, "defenders" ) )
						{
							// parse defenders chat
							pToken = COM_ParseExt( &pFile, TRUE );

							if( !pToken[0] )
							{
								G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
								bAborted = TRUE;
								break;
							}

							if( strcmp( pToken, "{" ) )
							{
								G_Printf("Parse Error : Missing { in botchat.cfg\n");
								bAborted = TRUE;
								break;
							}

							while(1)
							{
								pToken = COM_ParseExt( &pFile, TRUE );

								if( !pToken[0] )
								{
									G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
									bAborted = TRUE;
									break;
								}

								// read the defenders sentence
								strcpy( eventChats[ iNumEventChats ].szDefenders[ eventChats[ iNumEventChats ].iNumDefenders++ ], pToken );

								pToken = COM_ParseExt( &pFile, TRUE );

								if( !pToken[0] )
								{
									G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
									bAborted = TRUE;
									break;
								}

								if( !strcmp( pToken, "," ) )
								{
									continue;
								}
								else if( !strcmp( pToken, "}" ) )
								{
										// done parsing attackers chat stuff
										break;
								}
								else
								{
									G_Printf("Parse Error : Unknown token %s in botchat.cfg\n", pToken);
									bAborted = TRUE;
									break;
								}
							}
						}
					}
				}
				else if( !strcmp( pToken, "}" ) )
				{
					// done parsing EventChat section
					break;
				}
			}
		}
		else if( !Q_stricmp( pToken, "WordChat" ) )
		{
			// parse the word chat
			if( g_debugBots.integer )
				G_Printf("Parsing the Word Chat\n");
		}
		else if( !Q_stricmp( pToken, "WordLists" ) )
		{
			// parse the wordlists
			if( g_debugBots.integer )
				G_Printf("Parsing the Word Lists\n");

			pToken = COM_ParseExt( &pFile, TRUE );

			if( !pToken[0] )
			{
				G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			if( strcmp( pToken,"{" ) )
			{
				G_Printf("Parse Error : Missing { in botchat.cfg\n");
				bAborted = TRUE;
				break;
			}

			while(1)
			{
				// read the wordlist identifier
				pToken = COM_ParseExt( &pFile, TRUE );

				if( !pToken[0] )
				{
					G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				// done parsing wordLists
				if( !strcmp( pToken, "}" ) )
				{
					G_Printf("Done parsing WordLists\n");
					break;
				}

				// start with a #
				if( pToken[0] != '#' )
				{
					G_Printf("Parse Error : Wordlist does not start with a # in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				if( iNumWordLists >= MAX_CHAT_WORDLISTS )
				{
					G_Printf("Parse Error : iNumWordLists >= MAX_CHAT_WORDLISTS\n");
					bAborted = TRUE;
					break;
				}

				G_Printf("Reading WordList : %s\n", pToken );

				// create the new list
				Q_strncpyz( wordLists[ iNumWordLists ].szIdentifier, pToken, sizeof( wordLists[ iNumWordLists ].szIdentifier ) );

				pToken = COM_ParseExt( &pFile, TRUE );

				if( !pToken[0] )
				{
					G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				if( strcmp( pToken, "{" ) )
				{
					G_Printf("Parse Error : Missing { in botchat.cfg\n");
					bAborted = TRUE;
					break;
				}

				// read the words
				while(1)
				{
					pToken = COM_ParseExt( &pFile, TRUE );

					if( !pToken[0] )
					{
						G_Printf("Parse Error : Unexpected end of file in botchat.cfg\n");
						bAborted = TRUE;
						break;
					}

					if( !strcmp( pToken, "," ) )
					{
						continue;
					}
					else if( !strcmp( pToken, "}" ) )
					{
						// done reading words for this word list assume everything went ok
						iNumWordLists++;

						break;
					}
					else
					{
						// read the word
						char *pWord;

						wordLists[ iNumWordLists ].iNumWords++;

						// pointer array um einen char** pointer vergroessern
						wordLists[ iNumWordLists ].pWords = (char**) realloc( wordLists[ iNumWordLists ].pWords, sizeof(char**) * wordLists[ iNumWordLists ].iNumWords );

						// speicherplatz für das wort allozieren
						pWord = (char*) malloc( sizeof(char) * strlen(pToken) + 1 );

						// token in den speicherplatz kopieren
						strcpy( pWord, pToken );

						// pointer auf den allozierten speicherplatz verweisen lassen
						wordLists[ iNumWordLists ].pWords[ wordLists[ iNumWordLists ].iNumWords - 1 ] = pWord;
					}

				}


			}





		}
	}

	if( bAborted == FALSE )
	{
		G_Printf("Successfully read in botchat.cfg\n");
		bChatFileLoaded = TRUE;
	}

	trap_FS_FCloseFile(fHandle);
}

//
// Unloads chat file and frees up all allocated memory from chat code
//

void Bot_UnloadChatFile( void )
{
	int i;
	int y;
	wordList_t *pWordList;

	if( !bChatFileLoaded )
		return;

	// free up the wordlists
	for( i = 0; i < iNumWordLists; i++ )
	{
		pWordList = &wordLists[i];

		for( y = 0; y < pWordList->iNumWords; y++ )
		{
			free( *(pWordList->pWords + y) );
		}

		// now free the array of char** pointers
		free( pWordList->pWords );
	}

}

//
// Dumps all wordlists
//

void Bot_DumpWordLists( void )
{
	int i, y;

	for( i = 0; i < iNumWordLists; i++ )
	{
		G_Printf("WordList Identifier : %s\n", wordLists[i].szIdentifier );
		G_Printf("Words :\n");

		for( y = 0; y < wordLists[i].iNumWords; y++ )
		{
			G_Printf("%s\n", wordLists[i].pWords[y]);
		}

		G_Printf("\n");
	}
}

//
// Returns client name from clientNum
//

char *Bot_ClientName ( int client, char *name, int size ) 
{
	char buf[MAX_INFO_STRING];

	if( client < 0 || client >= MAX_CLIENTS )
	{
		G_Printf("Bot_ClientName : ClientName : client out of range\n");
		return "[client out of range]";
	}

	trap_GetConfigstring( CS_PLAYERS + client, buf, sizeof(buf) );
	strncpy( name, Info_ValueForKey(buf, "n"), size - 1 );
	name[ size - 1 ] = '\0';
	
	Q_CleanStr( name );

	return name;
}

//
// Returns cleaned client name without any clan tags or color escapes
//

char *Bot_CleanClientName( int client, char *buf, int size ) 
{
	int i;
	char *str1, *str2, *ptr, c;
	char name[128];

	strcpy( name, Bot_ClientName(client, name, sizeof(name)) );

	for (i = 0; name[i]; i++) 
		name[i] &= 127;
	
	//remove all spaces
	for( ptr = strstr(name, " "); ptr; ptr = strstr(name, " ") ) 
	{
		memmove( ptr, ptr+1, strlen(ptr+1) + 1 );
	}

	//check for [x] and ]x[ clan names
	str1 = strstr(name, "[");
	str2 = strstr(name, "]");

	if( str1 && str2 ) 
	{
		if( str2 > str1 ) 
			memmove( str1, str2+1, strlen(str2+1) + 1 );
		else 
			memmove( str2, str1+1, strlen(str1+1) + 1 );
	}

	//only allow lower case alphabet characters
	ptr = name;

	while(*ptr) 
	{
		c = *ptr;
	
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') 
		{
			ptr++;
		}
		else if (c >= 'A' && c <= 'Z')
		{
			*ptr += 'a' - 'A';
			ptr++;
		}
		else 
		{
			memmove(ptr, ptr+1, strlen(ptr + 1)+1);
		}
	}

	strncpy(buf, name, size-1);
	buf[size-1] = '\0';

	return buf;
}

//
// Returns name of a random teammate of bot
//

char *Bot_RandomTeammateName( gentity_t *ent, char *buf, int size )
{
	int teammates[MAX_CLIENTS];
	int nummates;
	int i;
	gclient_t *cl;

	nummates = 0;

	for( i = 0; i < g_maxclients.integer; i++ )
	{
		cl = level.clients + i;

		if( cl->ps.clientNum == ent->client->ps.clientNum )
			continue;

		if( cl->pers.connected != CON_CONNECTED )
			continue;

		if( cl->sess.sessionTeam != ent->client->sess.sessionTeam )
			continue;

		teammates[nummates++] = cl->ps.clientNum;
	}
	
	if( nummates == 0 )
	{
		return " ";
	}

	i = teammates[ rand() % nummates ];

	Bot_CleanClientName( i, buf, size );

	return buf;
}

//
// Returns name of a random enemy
//

char *Bot_RandomEnemyName( gentity_t *ent, char *buf, int size )
{
	int enemies[MAX_CLIENTS];
	int numenemies;
	int i;
	gclient_t *cl;

	numenemies = 0;

	for( i = 0; i < g_maxclients.integer; i++ )
	{
		cl = level.clients + i;

		if( cl->ps.clientNum == ent->client->ps.clientNum )
			continue;

		if( cl->pers.connected != CON_CONNECTED )
			continue;

		if( cl->sess.sessionTeam == ent->client->sess.sessionTeam )
			continue;

		enemies[numenemies++] = cl->ps.clientNum;
	}

	if( numenemies == 0 )
	{
		return " ";
	}

	i = enemies[ rand() % numenemies ];

	Bot_CleanClientName( i, buf, size );

	return buf;
}

//
// Returns the name of the player with the highest score
//

char *Bot_BestPlayerName( void )
{
	return NULL;
}

//
// Processes a chat message before output. Resolves all symbols to the actual values.
//

void Bot_ProcessChatMessage( gentity_t *ent, char *output, const char *input )
{
	char *pPattern;
	char *pStart;
	char szTemp[1024];
	int iLen;

	pPattern = (char*)&input[0];
	pStart = (char*)&input[0];

	output[0] = '\0';

	while( pPattern )
	{
		pPattern = strstr( pStart, "$" );

		if( pPattern == NULL )
		{
			strcat( output, pStart );
			break;
		}
		else
		{
			iLen = pPattern - pStart;

			strncpy( szTemp, pStart, iLen );
			szTemp[iLen] = '\0';
			strcat( output, szTemp );

			pPattern++;

			if( *pPattern == 'b' )
			{
				// b = Name of the bot
				char szBotName[128];

				// make sure name is cleaned up	
				Bot_CleanClientName( ent->client->ps.clientNum, szBotName, sizeof(szBotName) );

				strcat( output, szBotName );
			}
			else if( *pPattern == 'm' )
			{
				// m = Name of the current map
				char szMapName[128];
				char serverinfo[MAX_INFO_STRING];

				trap_GetServerinfo( serverinfo, sizeof(serverinfo) );
				Q_strncpyz( szMapName, Info_ValueForKey( serverinfo, "mapname" ), sizeof(szMapName) );

				strcat( output, szMapName );
			}
			else if( *pPattern == 'c' )
			{
				// c = Name of a random teammate
				char szTeammate[128];

				Bot_RandomTeammateName( ent, szTeammate, sizeof(szTeammate) );
				strcat( output, szTeammate );
			}
			else if( *pPattern == 'e' )
			{
				// e = Name of random enemy
				char szEnemy[128];

				Bot_RandomEnemyName( ent, szEnemy, sizeof(szEnemy) );
				strcat( output, szEnemy );
			}
			else if( *pPattern == 't' )
			{
				// t = time left to play
			}
			else if( *pPattern == 'h' )
			{
				// h = Name of player with highest score
			}

			pPattern++;
			pStart = pPattern;
		}
	}
}

//
// Returns a random chat record from the szGeneralChat array(s).
//

char *Bot_GetRandomChatRecord( gentity_t *ent )
{
	int team = ent->client->sess.sessionTeam;
	int total;
	int random;
	float ratio;

	if( !bChatFileLoaded )
		return "[error]";

	if( team == TEAM_AXIS )
	{
		// calculate the probability of using an axis only chat
		total = iGeneralChatCount[BOTH_CHAT] + iGeneralChatCount[AXIS_CHAT];

		ratio = (float) iGeneralChatCount[AXIS_CHAT] / total;
		ratio = (int) (ratio * 100);

		// pick a random record from the axis chats
		if( (rand() % 100) < ratio )
		{
			random = rand() % iGeneralChatCount[AXIS_CHAT];

			return &szGeneralChat[AXIS_CHAT][random][0];
		}
		else
		{
			random = rand() % iGeneralChatCount[BOTH_CHAT];

			return &szGeneralChat[BOTH_CHAT][random][0];
		}
	}
	else if( team == TEAM_ALLIES )
	{
		total = iGeneralChatCount[BOTH_CHAT] + iGeneralChatCount[ALLIES_CHAT];

		ratio = (float) iGeneralChatCount[ALLIES_CHAT] / total;
		ratio = (int) (ratio * 100);

		if( (rand() % 100) < ratio )
		{
			random = rand() % iGeneralChatCount[ALLIES_CHAT];

			return &szGeneralChat[ALLIES_CHAT][random][0];
		}
		else
		{
			random = rand() % iGeneralChatCount[BOTH_CHAT];

			return &szGeneralChat[BOTH_CHAT][random][0];
		}
	}

	return NULL;
}


