//**********************************************************************;
// Copyright (c) 2015, Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//**********************************************************************;

//
// tpmclient.cpp : Defines the entry point for the console test application.
//

#ifdef _WIN32
#include "stdafx.h"
#else
#include <stdarg.h>
#endif

#ifndef UNICODE
#define UNICODE 1
#endif

#ifdef _WIN32
// link with Ws2_32.lib
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#else
#define sprintf_s   snprintf
#define sscanf_s    sscanf
#endif

#include <stdio.h>
#include <string.h>

#include <sapi/tpm20.h>
#include <tcti/tcti_device.h>
#include "tpmtcticlient.h"
#include "tcti_util.h"
#include "debug.h"

int loadDataFromFile(const char *fileName, UINT8 *buf, UINT16 *size)
{
    UINT16 count = 1, left;
    FILE *f;
    if ( size == NULL || buf == NULL || fileName == NULL )
        return -1;

    f = fopen(fileName, "rb+");
    if( f == NULL )
    {
        printf("File(%s) open error.\n", fileName);
        return -2;
    }

    left = *size;
    *size = 0;
    while( left > 0 && count > 0 )
    {
        count = fread(buf, 1, left, f);
        *size += count;
        left -= count;
        buf += count;
    }

    if( *size == 0 )
    {
        printf("File read error\n");
        fclose(f);
        return -3;
    }
    fclose(f);
    return 0;
}

int saveDataToFile(const char *fileName, UINT8 *buf, UINT16 size)
{
    FILE *f;
    UINT16 count = 1;
    if( fileName == NULL || buf == NULL || size == 0 )
        return -1;

    f = fopen(fileName, "wb+");
    if( f == NULL )
    {
        printf("File(%s) open error.\n", fileName);
        return -2;
    }

    while( size > 0 && count > 0 )
    {
        count = fwrite(buf, 1, size, f);
        size -= count;
        buf += count;
    }

    if( size > 0 )
    {
        printf("File write error\n");
        fclose(f);
        return -3;
    }

    fclose(f);
    return 0;
}

void
hexdump (unsigned long bse, UINT8 *buf, int len)
{
  int pos;
  char line[80];

  while (len > 0)
    {
      int cnt, i;

      pos = snprintf (line, sizeof (line), "%08lx  ", bse);
      cnt = 16;
      if (cnt > len)
	cnt = len;

      for (i = 0; i < cnt; i++)
	{
	  pos += snprintf (&line[pos], sizeof (line) - pos,
				"%02x ", (unsigned char) buf[i]);
	  if ((i & 7) == 7)
	    line[pos++] = ' ';
	}

      for (; i < 16; i++)
	{
	  pos += snprintf (&line[pos], sizeof (line) - pos, "   ");
	  if ((i & 7) == 7)
	    line[pos++] = ' ';
	}

      line[pos++] = '|';

      for (i = 0; i < cnt; i++)
	line[pos++] = ((buf[i] >= 32) && (buf[i] < 127)) ? buf[i] : '.';

      line[pos++] = '|';

      line[pos] = 0;

      printf ("%s\n", line);

      /* Print only first and last line if more than 3 lines are identical.  */
      if (len >= 4 * 16
	  && ! memcmp (buf, buf + 1 * 16, 16)
	  && ! memcmp (buf, buf + 2 * 16, 16)
	  && ! memcmp (buf, buf + 3 * 16, 16))
	{
	  printf ("*\n");
	  do
	    {
	      bse += 16;
	      buf += 16;
	      len -= 16;
	    }
	  while (len >= 3 * 16 && ! memcmp (buf, buf + 2 * 16, 16));
	}

      bse += 16;
      buf += 16;
      len -= cnt;
    }
}


#if __linux || __unix
int ExecuteTPMCommand(char* input_name)
{
    TCTI_DEVICE_CONF deviceTctiConfig = {
        "/dev/tpm0",
        DebugPrintfCallback,
        NULL
    };
    TSS2_RC rval = TSS2_RC_SUCCESS;
    const char *deviceTctiName = "Local Device TCTI";
    UINT8* input_buffer;
    UINT16 size = 65535;
    input_buffer = (UINT8*) malloc(size);

    TSS2_TCTI_CONTEXT *downstreamTctiContext;

    // Init downstream interface to tpm (in this case the local TPM).
    rval = InitDeviceTctiContext( &deviceTctiConfig, &downstreamTctiContext, deviceTctiName );
    if( rval != TSS2_RC_SUCCESS )
    {
        DebugPrintf( NO_PREFIX,  "Resource Mgr, %s, failed initialization: 0x%x.  Exiting...\n", "local TPM", rval );
        return -1;
    }
    else
    {
        // Load from TPM binary data
        loadDataFromFile(input_name, input_buffer, &size);
        printf("    [*] Input Size %d\n", size);
        hexdump(0, input_buffer, size);

        // Execute TPM command
        rval = ( (TSS2_TCTI_CONTEXT_COMMON_CURRENT *)downstreamTctiContext )->transmit( downstreamTctiContext, size, input_buffer );
        if (rval != TSS2_RC_SUCCESS)
        {
            printf("Transmit Fail ret 0x%X\n", rval);
        }

        // Get TPM response
        rval = ( (TSS2_TCTI_CONTEXT_COMMON_CURRENT *)downstreamTctiContext )->receive(downstreamTctiContext, (size_t*)&size, input_buffer, TSS2_TCTI_TIMEOUT_BLOCK );
        if (rval != TSS2_RC_SUCCESS)
        {
            printf("Receive Fail ret 0x%X\n", rval);
        }

        // Print and dump TPM response
        printf("\n    [*] Output Size %d, ", size);
        if (*((uint32_t*)(input_buffer + 6)) == 0)
        {
            printf("Result: Success\n");
        }
        else
        {
            printf("Result: Fail!\n");
        }
        hexdump(0, input_buffer, size);

        saveDataToFile("out.bin", input_buffer, size);

        TeardownTctiContext( &downstreamTctiContext );
    }

    return 0;
}
#endif

char version[] = "1.0";

void PrintHelp()
{
    printf( "ex> tpmtcticlient [-i filename of TPM command binary]\n");
}

int main(int argc, char* argv[])
{
    char* input_name;

    setvbuf (stdout, NULL, _IONBF, BUFSIZ);

    if( argc != 3 )
    {
        PrintHelp();
        return 1;
    }
    else
    {
        if( 0 == strcmp( argv[1], "-i" ) )
        {
            input_name = argv[2];
        }
        else
        {
            PrintHelp();
            return 1;
        }
    }

#if __linux || __unix
    printf("Input file %s\n", input_name);
    return ExecuteTPMCommand(input_name);
#endif
}
