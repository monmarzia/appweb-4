/*
    sslModule.c - Module for SSL support
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "appweb.h"

#if BIT_PACK_SSL
/*********************************** Code *************************************/

static bool checkSsl(MaState *state)
{
    if (state->route->ssl == 0) {
        if (state->route->parent && state->route->parent->ssl) {
            state->route->ssl = mprCloneSsl(state->route->parent->ssl);
        } else {
            state->route->ssl = mprCreateSsl(1);
        }
    }
    return 1;
}


/*
    ListenSecure ip:port
    ListenSecure ip
    ListenSecure port

    Where ip may be "::::::" for ipv6 addresses or may be enclosed in "[]" if appending a port.
 */
static int listenSecureDirective(MaState *state, cchar *key, cchar *value)
{
#if BIT_PACK_SSL
    HttpEndpoint    *endpoint;
    char            *ip;
    int             port;

    mprParseSocketAddress(value, &ip, &port, HTTP_DEFAULT_PORT);
    if (port == 0) {
        mprError("Bad or missing port %d in ListenSecure directive", port);
        return -1;
    }
    endpoint = httpCreateEndpoint(ip, port, NULL);
    mprAddItem(state->server->endpoints, endpoint);
    if (state->route->ssl == 0) {
        if (state->route->parent && state->route->parent->ssl) {
            state->route->ssl = mprCloneSsl(state->route->parent->ssl);
        } else {
            state->route->ssl = mprCreateSsl(1);
        }
    }
    httpSecureEndpoint(endpoint, state->route->ssl);
    if (!state->host->secureEndpoint) {
        httpSetHostSecureEndpoint(state->host, endpoint);
    }
    return 0;
#else
    mprError("Configuration lacks SSL support");
    return -1;
#endif
}


static int sslCaCertificatePathDirective(MaState *state, cchar *key, cchar *value)
{
    char *path;
    if (!maTokenize(state, value, "%P", &path)) {
        return MPR_ERR_BAD_SYNTAX;
    }

    checkSsl(state);
    mprSetSslCaPath(state->route->ssl, path);
    return 0;
}


static int sslCaCertificateFileDirective(MaState *state, cchar *key, cchar *value)
{
    char *path;
    if (!maTokenize(state, value, "%P", &path)) {
        return MPR_ERR_BAD_SYNTAX;
    }

    checkSsl(state);
    mprSetSslCaFile(state->route->ssl, path);
    return 0;
}


static int sslCertificateFileDirective(MaState *state, cchar *key, cchar *value)
{
    char *path;
    if (!maTokenize(state, value, "%P", &path)) {
        return MPR_ERR_BAD_SYNTAX;
    }

    checkSsl(state);
    mprSetSslCertFile(state->route->ssl, path);
    return 0;
}


static int sslCertificateKeyFileDirective(MaState *state, cchar *key, cchar *value)
{
    char *path;
    if (!maTokenize(state, value, "%P", &path)) {
        return MPR_ERR_BAD_SYNTAX;
    }

    checkSsl(state);
    mprSetSslKeyFile(state->route->ssl, path);
    return 0;
}


static int sslCipherSuiteDirective(MaState *state, cchar *key, cchar *value)
{
    checkSsl(state);
    mprSetSslCiphers(state->route->ssl, value);
    return 0;
}


/*
    SSLProvider [provider]
 */
static int sslProviderDirective(MaState *state, cchar *key, cchar *value)
{
    char    *provider;

    if (!maTokenize(state, value, "?S", &provider)) {
        return MPR_ERR_BAD_SYNTAX;
    }
    checkSsl(state);
    mprSetSslProvider(state->route->ssl, provider);
    return 0;
}


/*
    SSLEngine on [provider]
 */
static int sslEngineDirective(MaState *state, cchar *key, cchar *value)
{
    char    *provider;
    bool    on;

    if (!maTokenize(state, value, "%B ?S", &on, &provider)) {
        return MPR_ERR_BAD_SYNTAX;
    }
    if (on) {
        checkSsl(state);
        mprSetSslProvider(state->route->ssl, provider);
        if (!state->host->secureEndpoint) {
            if (httpSecureEndpointByName(state->host->name, state->route->ssl) < 0) {
                mprError("No HttpEndpoint at %s to secure. Must use inside a VirtualHost block", state->host->name);
                return MPR_ERR_BAD_STATE;
            }
        }
    }
    return 0;
}


static int sslVerifyClientDirective(MaState *state, cchar *key, cchar *value)
{
    checkSsl(state);
    if (scaselesscmp(value, "require") == 0) {
        mprVerifySslPeer(state->route->ssl, 1);

    } else if (scaselesscmp(value, "none") == 0) {
        mprVerifySslPeer(state->route->ssl, 0);

    } else {
        mprError("Unknown verify client option");
        return MPR_ERR_BAD_STATE;
    }
    return 0;
}


static int sslVerifyDepthDirective(MaState *state, cchar *key, cchar *value)
{
    checkSsl(state);
    mprVerifySslDepth(state->route->ssl, (int) stoi(value));
    return 0;
}


static int sslVerifyIssuerDirective(MaState *state, cchar *key, cchar *value)
{
    bool    on;

    checkSsl(state);
    if (!maTokenize(state, value, "%B", &on)) {
        return MPR_ERR_BAD_SYNTAX;
    }
    mprVerifySslIssuer(state->route->ssl, on);
    return 0;
}


static int sslProtocolDirective(MaState *state, cchar *key, cchar *value)
{
    char    *word, *tok;
    int     mask, protoMask;

    checkSsl(state);
    protoMask = 0;
    word = stok(sclone(value), " \t", &tok);
    while (word) {
        mask = -1;
        if (*word == '-') {
            word++;
            mask = 0;
        } else if (*word == '+') {
            word++;
        }
        if (scaselesscmp(word, "SSLv2") == 0) {
            protoMask &= ~(MPR_PROTO_SSLV2 & ~mask);
            protoMask |= (MPR_PROTO_SSLV2 & mask);

        } else if (scaselesscmp(word, "SSLv3") == 0) {
            protoMask &= ~(MPR_PROTO_SSLV3 & ~mask);
            protoMask |= (MPR_PROTO_SSLV3 & mask);

        } else if (scaselesscmp(word, "TLSv1") == 0) {
            protoMask &= ~(MPR_PROTO_TLSV1 & ~mask);
            protoMask |= (MPR_PROTO_TLSV1 & mask);

        } else if (scaselesscmp(word, "ALL") == 0) {
            protoMask &= ~(MPR_PROTO_ALL & ~mask);
            protoMask |= (MPR_PROTO_ALL & mask);
        }
        word = stok(0, " \t", &tok);
    }
    mprSetSslProtocols(state->route->ssl, protoMask);
    return 0;
}


/*
    Loadable module initialization. 
 */
PUBLIC int maSslModuleInit(Http *http, MprModule *module)
{
    HttpStage   *stage;
    MaAppweb    *appweb;

    if ((stage = httpCreateStage(http, "sslModule", HTTP_STAGE_MODULE, module)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    appweb = httpGetContext(http);
    maAddDirective(appweb, "ListenSecure", listenSecureDirective);
    maAddDirective(appweb, "SSLEngine", sslEngineDirective);
    maAddDirective(appweb, "SSLCACertificateFile", sslCaCertificateFileDirective);
    maAddDirective(appweb, "SSLCACertificatePath", sslCaCertificatePathDirective);
    maAddDirective(appweb, "SSLCertificateFile", sslCertificateFileDirective);
    maAddDirective(appweb, "SSLCertificateKeyFile", sslCertificateKeyFileDirective);
    maAddDirective(appweb, "SSLCipherSuite", sslCipherSuiteDirective);
    maAddDirective(appweb, "SSLProtocol", sslProtocolDirective);
    maAddDirective(appweb, "SSLProvider", sslProviderDirective);
    maAddDirective(appweb, "SSLVerifyClient", sslVerifyClientDirective);
    maAddDirective(appweb, "SSLVerifyDepth", sslVerifyDepthDirective);
    maAddDirective(appweb, "SSLVerifyIssuer", sslVerifyIssuerDirective);

    return 0;
}
#else

PUBLIC int maSslModuleInit(Http *http, MprModule *mp)
{
    return 0;
}
#endif /* BIT_PACK_SSL */

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
