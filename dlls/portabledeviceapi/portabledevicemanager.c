/* IPortableDeviceManager Implementation
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
#include "wine/debug.h"
#include "wine/heap.h"

#include "portabledevicemanager_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(portdevmgr);

/*****************************************************************************
 * IPortableDeviceManagerImpl implementation
 */
typedef struct IPortableDeviceManagerImpl {
    IPortableDeviceManager IPortableDeviceManager_iface;
    LONG ref;
} IPortableDeviceManagerImpl;

static ULONG WINAPI PortableDeviceManager_AddRef(IPortableDeviceManager *iface);

static inline IPortableDeviceManagerImpl *impl_from_IPortableDeviceManager(IPortableDeviceManager *iface)
{
    return CONTAINING_RECORD(iface, IPortableDeviceManagerImpl, IPortableDeviceManager_iface);
}

static HRESULT WINAPI PortableDeviceManager_QueryInterface(IPortableDeviceManager *iface, REFIID riid, void **ret_iface)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);

    TRACE("(%p, %p)\n", This, ret_iface);

    *ret_iface = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IPortableDeviceManager)) {
        *ret_iface = &This->IPortableDeviceManager_iface;
    } else {
        WARN("(%p, %p): not found\n", This, ret_iface);
        return E_NOINTERFACE;
    }

    IPortableDeviceManager_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI PortableDeviceManager_AddRef(IPortableDeviceManager *iface)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI PortableDeviceManager_Release(IPortableDeviceManager *iface)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if (!ref) {
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI PortableDeviceManager_GetDevices(IPortableDeviceManager *iface,
        WCHAR **pnp_device_ids,
        DWORD *len)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI PortableDeviceManager_RefreshDeviceList(IPortableDeviceManager *iface)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI PortableDeviceManager_GetDeviceFriendlyName(IPortableDeviceManager *iface,
        const WCHAR *pnp_device_id,
        WCHAR *device_friendly_name,
        DWORD *len)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI PortableDeviceManager_GetDeviceDescription(IPortableDeviceManager *iface,
        const WCHAR *pnp_device_id,
        WCHAR *device_description,
        DWORD *len)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI PortableDeviceManager_GetDeviceManufacturer(IPortableDeviceManager *iface,
        const WCHAR *pnp_device_id,
        WCHAR *device_manufacturer,
        DWORD *len)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI PortableDeviceManager_GetDeviceProperty(IPortableDeviceManager *iface,
        const WCHAR *pnp_device_id,
        const WCHAR *device_property_name,
        BYTE *data,
        DWORD *len,
        DWORD *type)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI PortableDeviceManager_GetPrivateDevices(IPortableDeviceManager *iface,
        WCHAR **pnp_device_ids,
        DWORD *len)
{
    IPortableDeviceManagerImpl *This = impl_from_IPortableDeviceManager(iface);
    return E_NOTIMPL;
}

static const IPortableDeviceManagerVtbl portabledevicemanager_vtbl = {
    PortableDeviceManager_QueryInterface,
    PortableDeviceManager_AddRef,
    PortableDeviceManager_Release,
    PortableDeviceManager_GetDevices,
    PortableDeviceManager_RefreshDeviceList,
    PortableDeviceManager_GetDeviceFriendlyName,
    PortableDeviceManager_GetDeviceDescription,
    PortableDeviceManager_GetDeviceManufacturer,
    PortableDeviceManager_GetDeviceProperty,
    PortableDeviceManager_GetPrivateDevices
};

/* for ClassFactory */
HRESULT create_portabledevicemanager(REFIID iid, void **obj)
{
    IPortableDeviceManagerImpl *This;
    HRESULT hr;

    if (!(This = heap_alloc(sizeof(*This))))
        return E_OUTOFMEMORY;

    This->IPortableDeviceManager_iface.lpVtbl = &portabledevicemanager_vtbl;
    This->ref = 1;

    hr = IPortableDeviceManager_QueryInterface(&This->IPortableDeviceManager_iface, iid, obj);
    IPortableDeviceManager_Release(&This->IPortableDeviceManager_iface);
    return hr;
}
