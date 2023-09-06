/* Portable Device Manager Main
 *
 * Copyright 2023 Corentin Noël
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#include <stdio.h>
#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "olectl.h"
#include "rpcproxy.h"
#include "wine/debug.h"

#include "portabledevicemanager_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(portdevmgr);

/******************************************************************
 *              PortableDeviceManager ClassFactory
 */


/***********************************************************
 *    ClassFactory implementation
 */
typedef HRESULT (*CreateInstanceFunc)(IUnknown*,REFIID,void**);

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFGUID riid, void **ppvObject)
{
    if(IsEqualGUID(&IID_IClassFactory, riid) || IsEqualGUID(&IID_IUnknown, riid)) {
        IClassFactory_AddRef(iface);
        *ppvObject = iface;
        return S_OK;
    }

    WARN("not supported iid %s\n", debugstr_guid(riid));
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    TRACE("(%p)\n", iface);
    return 2;
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    TRACE("(%p)\n", iface);

    return 1;
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **ppv)
{
    TRACE("(%p %s %p)\n", outer, debugstr_guid(riid), ppv);
    return create_portabledevicemanager (riid, ppv);
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL fLock)
{
    TRACE("(%p)->(%x)\n", iface, fLock);
    return S_OK;
}

static const IClassFactoryVtbl PDMClassFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ClassFactory_CreateInstance,
    ClassFactory_LockServer
};

static IClassFactory PDMVersionInfoFactory = { &PDMClassFactoryVtbl };

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources();
}

HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources();
}

HRESULT WINAPI DllGetClassObject( REFCLSID riid, REFIID iid, LPVOID *ppv )
{
    TRACE("%s %s %p\n", debugstr_guid(riid), debugstr_guid(iid), ppv );

    if( IsEqualCLSID( riid, &CLSID_PortableDeviceManager ))
    {
        TRACE("(CLSID_PortableDeviceManager %s %p)\n", debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&PDMVersionInfoFactory, iid, ppv);
    }

    FIXME("Unsupported interface %s\n", debugstr_guid(riid));
    return CLASS_E_CLASSNOTAVAILABLE;
}
