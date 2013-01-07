/*
 * Copyright 2011-2013 André Hentschel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define NONAMELESSUNION
#include "netstat.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(netstat);

static const WCHAR ipW[] = {'I', 'P', 0};
static const WCHAR ipv6W[] = {'I', 'P', 'v', '6', 0};
static const WCHAR icmpW[] = {'I', 'C', 'M', 'P', 0};
static const WCHAR icmpv6W[] = {'I', 'C', 'M', 'P', 'v', '6', 0};
static const WCHAR tcpW[] = {'T', 'C', 'P', 0};
static const WCHAR tcpv6W[] = {'T', 'C', 'P', 'v', '6', 0};
static const WCHAR udpW[] = {'U', 'D', 'P', 0};
static const WCHAR udpv6W[] = {'U', 'D', 'P', 'v', '6', 0};

static const WCHAR fmtport[] = {'%', 'd', 0};
static const WCHAR fmtip[] = {'%', 'd', '.', '%', 'd', '.', '%', 'd', '.', '%', 'd', 0};
static const WCHAR fmtn[] = {'\n', 0};
static const WCHAR fmtnn[] = {'\n', '%', 's', '\n', 0};
static const WCHAR fmtcolon[] = {'%', 's', ':', '%', 's', 0};
static const WCHAR fmttcpout[] = {' ', ' ', '%', '-', '6', 's', ' ', '%', '-', '2', '2', 's', ' ', '%', '-', '2', '2', 's', ' ', '%', 's', '\n', 0};
static const WCHAR fmtudpout[] = {' ', ' ', '%', '-', '6', 's', ' ', '%', '-', '2', '2', 's', ' ', '*', ':', '*', '\n', 0};
static const WCHAR fmtethout[] = {'%', '-', '2', '0', 's', ' ', '%', '1', '4', 'l', 'u', ' ', '%', '1', '5', 'l', 'u', '\n', 0};
static const WCHAR fmtethoutu[] = {'%', '-', '2', '0', 's', ' ', '%', '1', '4', 'l', 'u', '\n', '\n', 0};
static const WCHAR fmtethheader[] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                                     ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                                     ' ', '%', '-', '1', '9', 's', ' ', '%', 's', '\n', '\n', 0};

static const WCHAR tcpstatesW[][16] = {
    {'?', '?', '?', 0},
    {'C', 'L', 'O', 'S', 'E', 'D', 0},
    {'L', 'I', 'S', 'T', 'E', 'N', 'I', 'N', 'G', 0},
    {'S', 'Y', 'N', '_', 'S', 'E', 'N', 'T', 0},
    {'S', 'Y', 'N', '_', 'R', 'C', 'V', 'D', 0},
    {'E', 'S', 'T', 'A', 'B', 'L', 'I', 'S', 'H', 'E', 'D', 0},
    {'F', 'I', 'N', '_', 'W', 'A', 'I', 'T', '1', 0},
    {'F', 'I', 'N', '_', 'W', 'A', 'I', 'T', '2', 0},
    {'C', 'L', 'O', 'S', 'E', '_', 'W', 'A', 'I', 'T', 0},
    {'C', 'L', 'O', 'S', 'I', 'N', 'G', 0},
    {'L', 'A', 'S', 'T', '_', 'A', 'C', 'K', 0},
    {'T', 'I', 'M', 'E', '_', 'W', 'A', 'I', 'T', 0},
    {'D', 'E', 'L', 'E', 'T', 'E', '_', 'T', 'C', 'B', 0},
};

/* =========================================================================
 *  Output a unicode string. Ideally this will go to the console
 *  and hence required WriteConsoleW to output it, however if file i/o is
 *  redirected, it needs to be WriteFile'd using OEM (not ANSI) format
 * ========================================================================= */
static int __cdecl NETSTAT_wprintf(const WCHAR *format, ...)
{
    static WCHAR *output_bufW = NULL;
    static char  *output_bufA = NULL;
    static BOOL  toConsole    = TRUE;
    static BOOL  traceOutput  = FALSE;
#define MAX_WRITECONSOLE_SIZE 65535

    __ms_va_list parms;
    DWORD   nOut;
    int len;
    DWORD   res = 0;

    /*
     * Allocate buffer to use when writing to console
     * Note: Not freed - memory will be allocated once and released when
     *         xcopy ends
     */

    if (!output_bufW) output_bufW = HeapAlloc(GetProcessHeap(), 0,
                                              MAX_WRITECONSOLE_SIZE);
    if (!output_bufW) {
        WINE_FIXME("Out of memory - could not allocate 2 x 64K buffers\n");
        return 0;
    }

    __ms_va_start(parms, format);
    len = wvsprintfW(output_bufW, format, parms);
    __ms_va_end(parms);

    /* Try to write as unicode all the time we think its a console */
    if (toConsole) {
        res = WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
                            output_bufW, len, &nOut, NULL);
    }

    /* If writing to console has failed (ever) we assume its file
       i/o so convert to OEM codepage and output                  */
    if (!res) {
        BOOL usedDefaultChar = FALSE;
        DWORD convertedChars;

        toConsole = FALSE;

        /*
         * Allocate buffer to use when writing to file. Not freed, as above
         */
        if (!output_bufA) output_bufA = HeapAlloc(GetProcessHeap(), 0,
                                                  MAX_WRITECONSOLE_SIZE);
        if (!output_bufA) {
            WINE_FIXME("Out of memory - could not allocate 2 x 64K buffers\n");
            return 0;
        }

        /* Convert to OEM, then output */
        convertedChars = WideCharToMultiByte(GetConsoleOutputCP(), 0, output_bufW,
                                             len, output_bufA, MAX_WRITECONSOLE_SIZE,
                                             "?", &usedDefaultChar);
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), output_bufA, convertedChars,
                  &nOut, FALSE);
    }

    /* Trace whether screen or console */
    if (!traceOutput) {
        WINE_TRACE("Writing to console? (%d)\n", toConsole);
        traceOutput = TRUE;
    }
    return nOut;
}

static WCHAR *NETSTAT_load_message(UINT id) {
    static WCHAR msg[2048];
    static const WCHAR failedW[]  = {'F','a','i','l','e','d','!','\0'};

    if (!LoadStringW(GetModuleHandleW(NULL), id, msg, sizeof(msg)/sizeof(WCHAR))) {
        WINE_FIXME("LoadString failed with %d\n", GetLastError());
        strcpyW(msg, failedW);
    }
    return msg;
}

static WCHAR *NETSTAT_port_name(UINT port, WCHAR name[])
{
    /* FIXME: can we get the name? */
    sprintfW(name, fmtport, htons((WORD)port));
    return name;
}

static WCHAR *NETSTAT_host_name(UINT ip, WCHAR name[])
{
    UINT nip;

    /* FIXME: can we get the name? */
    nip = htonl(ip);
    sprintfW(name, fmtip, (nip >> 24) & 0xFF, (nip >> 16) & 0xFF, (nip >> 8) & 0xFF, (nip) & 0xFF);
    return name;
}

static void NETSTAT_conn_header(void)
{
    WCHAR local[22], remote[22], state[22];
    NETSTAT_wprintf(fmtnn, NETSTAT_load_message(IDS_TCP_ACTIVE_CONN));
    NETSTAT_wprintf(fmtn);
    strcpyW(local, NETSTAT_load_message(IDS_TCP_LOCAL_ADDR));
    strcpyW(remote, NETSTAT_load_message(IDS_TCP_REMOTE_ADDR));
    strcpyW(state, NETSTAT_load_message(IDS_TCP_STATE));
    NETSTAT_wprintf(fmttcpout, NETSTAT_load_message(IDS_TCP_PROTO), local, remote, state);
}

static void NETSTAT_eth_stats(void)
{
    PMIB_IFTABLE table;
    DWORD err, size, i;
    DWORD octets[2], ucastpkts[2], nucastpkts[2], discards[2], errors[2], unknown;
    WCHAR recv[19];

    size = sizeof(MIB_IFTABLE);
    do
    {
        table = (PMIB_IFTABLE)HeapAlloc(GetProcessHeap(), 0, size);
        err = GetIfTable(table, &size, FALSE);
        if (err != NO_ERROR) HeapFree(GetProcessHeap(), 0, table);
    } while (err == ERROR_INSUFFICIENT_BUFFER);

    if (err) return;

    NETSTAT_wprintf(NETSTAT_load_message(IDS_ETH_STAT));
    NETSTAT_wprintf(fmtn);
    NETSTAT_wprintf(fmtn);
    strcpyW(recv, NETSTAT_load_message(IDS_ETH_RECV));
    NETSTAT_wprintf(fmtethheader, recv, NETSTAT_load_message(IDS_ETH_SENT));

    octets[0] = octets[1] = 0;
    ucastpkts[0] = ucastpkts[1] = 0;
    nucastpkts[0] = nucastpkts[1] = 0;
    discards[0] = discards[1] = 0;
    errors[0] = errors[1] = 0;
    unknown = 0;

    for (i = 0; i < table->dwNumEntries; i++)
    {
        octets[0] += table->table[i].dwInOctets;
        octets[1] += table->table[i].dwOutOctets;
        ucastpkts[0] += table->table[i].dwInUcastPkts;
        ucastpkts[1] += table->table[i].dwOutUcastPkts;
        nucastpkts[0] += table->table[i].dwInNUcastPkts;
        nucastpkts[1] += table->table[i].dwOutNUcastPkts;
        discards[0] += table->table[i].dwInDiscards;
        discards[1] += table->table[i].dwOutDiscards;
        errors[0] += table->table[i].dwInErrors;
        errors[1] += table->table[i].dwOutErrors;
        unknown += table->table[i].dwInUnknownProtos;
    }

    NETSTAT_wprintf(fmtethout, NETSTAT_load_message(IDS_ETH_BYTES), octets[0], octets[1]);
    NETSTAT_wprintf(fmtethout, NETSTAT_load_message(IDS_ETH_UNICAST), ucastpkts[0], ucastpkts[1]);
    NETSTAT_wprintf(fmtethout, NETSTAT_load_message(IDS_ETH_NUNICAST), nucastpkts[0], nucastpkts[1]);
    NETSTAT_wprintf(fmtethout, NETSTAT_load_message(IDS_ETH_DISCARDS), discards[0], discards[1]);
    NETSTAT_wprintf(fmtethout, NETSTAT_load_message(IDS_ETH_ERRORS), errors[0], errors[1]);
    NETSTAT_wprintf(fmtethoutu, NETSTAT_load_message(IDS_ETH_UNKNOWN), unknown);

    HeapFree(GetProcessHeap(), 0, table);
}

static void NETSTAT_tcp_table(void)
{
    PMIB_TCPTABLE table;
    DWORD err, size, i;
    WCHAR HostIp[MAX_HOSTNAME_LEN], HostPort[32];
    WCHAR RemoteIp[MAX_HOSTNAME_LEN], RemotePort[32];
    WCHAR Host[MAX_HOSTNAME_LEN + 32];
    WCHAR Remote[MAX_HOSTNAME_LEN + 32];

    size = sizeof(MIB_TCPTABLE);
    do
    {
        table = (PMIB_TCPTABLE)HeapAlloc(GetProcessHeap(), 0, size);
        err = GetTcpTable(table, &size, TRUE);
        if (err != NO_ERROR) HeapFree(GetProcessHeap(), 0, table);
    } while (err == ERROR_INSUFFICIENT_BUFFER);

    if (err) return;

    for (i = 0; i < table->dwNumEntries; i++)
    {
        if ((table->table[i].u.dwState ==  MIB_TCP_STATE_CLOSE_WAIT) ||
            (table->table[i].u.dwState ==  MIB_TCP_STATE_ESTAB) ||
            (table->table[i].u.dwState ==  MIB_TCP_STATE_TIME_WAIT))
        {
            NETSTAT_host_name(table->table[i].dwLocalAddr, HostIp);
            NETSTAT_port_name(table->table[i].dwLocalPort, HostPort);
            NETSTAT_host_name(table->table[i].dwRemoteAddr, RemoteIp);
            NETSTAT_port_name(table->table[i].dwRemotePort, RemotePort);

            sprintfW(Host, fmtcolon, HostIp, HostPort);
            sprintfW(Remote, fmtcolon, RemoteIp, RemotePort);
            NETSTAT_wprintf(fmttcpout, tcpW, Host, Remote, tcpstatesW[table->table[i].u.dwState]);
        }
    }
    HeapFree(GetProcessHeap(), 0, table);
}

static void NETSTAT_udp_table(void)
{
    PMIB_UDPTABLE table;
    DWORD err, size, i;
    WCHAR HostIp[MAX_HOSTNAME_LEN], HostPort[32];
    WCHAR Host[MAX_HOSTNAME_LEN + 32];

    size = sizeof(MIB_UDPTABLE);
    do
    {
        table = (PMIB_UDPTABLE)HeapAlloc(GetProcessHeap(), 0, size);
        err = GetUdpTable(table, &size, TRUE);
        if (err != NO_ERROR) HeapFree(GetProcessHeap(), 0, table);
    } while (err == ERROR_INSUFFICIENT_BUFFER);

    if (err) return;

    for (i = 0; i < table->dwNumEntries; i++)
    {
        NETSTAT_host_name(table->table[i].dwLocalAddr, HostIp);
        NETSTAT_port_name(table->table[i].dwLocalPort, HostPort);

        sprintfW(Host, fmtcolon, HostIp, HostPort);
        NETSTAT_wprintf(fmtudpout, udpW, Host);
    }
    HeapFree(GetProcessHeap(), 0, table);
}

static NETSTATPROTOCOLS NETSTAT_get_protocol(WCHAR name[])
{
    if (!strcmpiW(name, ipW)) return PROT_IP;
    if (!strcmpiW(name, ipv6W)) return PROT_IPV6;
    if (!strcmpiW(name, icmpW)) return PROT_ICMP;
    if (!strcmpiW(name, icmpv6W)) return PROT_ICMPV6;
    if (!strcmpiW(name, tcpW)) return PROT_TCP;
    if (!strcmpiW(name, tcpv6W)) return PROT_TCPV6;
    if (!strcmpiW(name, udpW)) return PROT_UDP;
    if (!strcmpiW(name, udpv6W)) return PROT_UDPV6;
    return PROT_UNKNOWN;
}

int wmain(int argc, WCHAR *argv[])
{
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
    {
        WINE_ERR("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    if (argc == 1)
    {
        /* No options */
        NETSTAT_conn_header();
        NETSTAT_tcp_table();
        return 0;
    }

    while (argv[1] && argv[1][0] == '-')
    {
        switch (argv[1][1])
        {
        case 'a':
            NETSTAT_conn_header();
            NETSTAT_tcp_table();
            NETSTAT_udp_table();
            return 0;
        case 'e':
            NETSTAT_eth_stats();
            return 0;
        case 'p':
            argv++; argc--;
            if (argc == 1) return 1;
            switch (NETSTAT_get_protocol(argv[1]))
            {
                case PROT_TCP:
                    NETSTAT_conn_header();
                    NETSTAT_tcp_table();
                    break;
                case PROT_UDP:
                    NETSTAT_conn_header();
                    NETSTAT_udp_table();
                    break;
                default:
                    WINE_FIXME("Protocol not yet implemented: %s\n", debugstr_w(argv[1]));
            }
            return 0;
        default:
            WINE_FIXME("Unknown option: %s\n", debugstr_w(argv[1]));
            return 1;
        }
        argv++; argc--;
    }

    return 0;
}
