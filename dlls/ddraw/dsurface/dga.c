/*		DirectDrawSurface XF86DGA implementation
 *
 * Copyright 1997-2000 Marcus Meissner
 * Copyright 1998-2000 Lionel Ulmer (most of Direct3D stuff)
 */
#include "config.h"
#include "winerror.h"

#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debugtools.h"
#include "dga_private.h"

DEFAULT_DEBUG_CHANNEL(ddraw);

#define DDPRIVATE(x) dga_dd_private *ddpriv = ((dga_dd_private*)(x)->private)
#define DPPRIVATE(x) dga_dp_private *dppriv = ((dga_dp_private*)(x)->private)
#define DSPRIVATE(x) dga_ds_private *dspriv = ((dga_ds_private*)(x)->private)

/******************************************************************************
 *		IDirectDrawSurface methods
 *
 * Since DDS3 and DDS2 are supersets of DDS, we implement DDS3 and let
 * DDS and DDS2 use those functions. (Function calls did not change (except
 * using different DirectDrawSurfaceX version), just added flags and functions)
 */

HRESULT WINAPI DGA_IDirectDrawSurface4Impl_Flip(
    LPDIRECTDRAWSURFACE4 iface,LPDIRECTDRAWSURFACE4 flipto,DWORD dwFlags
) {
    ICOM_THIS(IDirectDrawSurface4Impl,iface);
    DSPRIVATE(This);
    dga_ds_private	*fspriv;
    IDirectDrawSurface4Impl* iflipto=(IDirectDrawSurface4Impl*)flipto;
    DWORD		xheight;
    LPBYTE		surf;

    TRACE("(%p)->Flip(%p,%08lx)\n",This,iflipto,dwFlags);
    iflipto = _common_find_flipto(This,iflipto);

    /* and flip! */
    fspriv = (dga_ds_private*)iflipto->private;
    TSXF86DGASetViewPort(display,DefaultScreen(display),0,fspriv->fb_height);
    if (iflipto->s.palette) {
	DPPRIVATE(iflipto);
	
	if (dppriv->cm)
	    TSXF86DGAInstallColormap(display,DefaultScreen(display),dppriv->cm);
    }
    while (!TSXF86DGAViewPortChanged(display,DefaultScreen(display),2)) {
	/* EMPTY */
    }
    /* We need to switch the lowlevel surfaces, for DGA this is: */

    /* The height within the framebuffer */
    xheight		= dspriv->fb_height;
    dspriv->fb_height	= fspriv->fb_height;
    fspriv->fb_height	= xheight;

    /* And the assciated surface pointer */
    surf				= This->s.surface_desc.u1.lpSurface;
    This->s.surface_desc.u1.lpSurface	= iflipto->s.surface_desc.u1.lpSurface;
    iflipto->s.surface_desc.u1.lpSurface= surf;
    return DD_OK;
}

HRESULT WINAPI DGA_IDirectDrawSurface4Impl_SetPalette(
    LPDIRECTDRAWSURFACE4 iface,LPDIRECTDRAWPALETTE pal
) {
    ICOM_THIS(IDirectDrawSurface4Impl,iface);
    DDPRIVATE(This->s.ddraw);
    IDirectDrawPaletteImpl* ipal=(IDirectDrawPaletteImpl*)pal;

    TRACE("(%p)->(%p)\n",This,ipal);

    /* According to spec, we are only supposed to 
     * AddRef if this is not the same palette.
     */
    if( This->s.palette != ipal ) {
	dga_dp_private	*fppriv;
	if( ipal != NULL )
	    IDirectDrawPalette_AddRef( (IDirectDrawPalette*)ipal );
	if( This->s.palette != NULL )
	    IDirectDrawPalette_Release( (IDirectDrawPalette*)This->s.palette );
	This->s.palette = ipal;
	fppriv = (dga_dp_private*)This->s.palette->private;
	ddpriv->InstallColormap(display,DefaultScreen(display),fppriv->cm);
    }
    return DD_OK;
}

ULONG WINAPI DGA_IDirectDrawSurface4Impl_Release(LPDIRECTDRAWSURFACE4 iface) {
    ICOM_THIS(IDirectDrawSurface4Impl,iface);
    DDPRIVATE(This->s.ddraw);
    DSPRIVATE(This);

    TRACE("(%p)->() decrementing from %lu.\n", This, This->ref );

    if (--(This->ref))
    	return This->ref;

    IDirectDraw2_Release((IDirectDraw2*)This->s.ddraw);
    /* clear out of surface list */
    if (ddpriv->fb_height == -1)
	HeapFree(GetProcessHeap(),0,This->s.surface_desc.u1.lpSurface);
    else
	ddpriv->vpmask &= ~(1<<(dspriv->fb_height/ddpriv->fb_height));

    /* Free the DIBSection (if any) */
    if (This->s.hdc != 0) {
	SelectObject(This->s.hdc, This->s.holdbitmap);
	DeleteDC(This->s.hdc);
	DeleteObject(This->s.DIBsection);
    }
    /* Free the clipper if attached to this surface */
    if( This->s.lpClipper )
	IDirectDrawClipper_Release(This->s.lpClipper);
    HeapFree(GetProcessHeap(),0,This);
    return S_OK;
}

ICOM_VTABLE(IDirectDrawSurface4) dga_dds4vt = 
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IDirectDrawSurface4Impl_QueryInterface,
    IDirectDrawSurface4Impl_AddRef,
    DGA_IDirectDrawSurface4Impl_Release,
    IDirectDrawSurface4Impl_AddAttachedSurface,
    IDirectDrawSurface4Impl_AddOverlayDirtyRect,
    IDirectDrawSurface4Impl_Blt,
    IDirectDrawSurface4Impl_BltBatch,
    IDirectDrawSurface4Impl_BltFast,
    IDirectDrawSurface4Impl_DeleteAttachedSurface,
    IDirectDrawSurface4Impl_EnumAttachedSurfaces,
    IDirectDrawSurface4Impl_EnumOverlayZOrders,
    DGA_IDirectDrawSurface4Impl_Flip,
    IDirectDrawSurface4Impl_GetAttachedSurface,
    IDirectDrawSurface4Impl_GetBltStatus,
    IDirectDrawSurface4Impl_GetCaps,
    IDirectDrawSurface4Impl_GetClipper,
    IDirectDrawSurface4Impl_GetColorKey,
    IDirectDrawSurface4Impl_GetDC,
    IDirectDrawSurface4Impl_GetFlipStatus,
    IDirectDrawSurface4Impl_GetOverlayPosition,
    IDirectDrawSurface4Impl_GetPalette,
    IDirectDrawSurface4Impl_GetPixelFormat,
    IDirectDrawSurface4Impl_GetSurfaceDesc,
    IDirectDrawSurface4Impl_Initialize,
    IDirectDrawSurface4Impl_IsLost,
    IDirectDrawSurface4Impl_Lock,
    IDirectDrawSurface4Impl_ReleaseDC,
    IDirectDrawSurface4Impl_Restore,
    IDirectDrawSurface4Impl_SetClipper,
    IDirectDrawSurface4Impl_SetColorKey,
    IDirectDrawSurface4Impl_SetOverlayPosition,
    DGA_IDirectDrawSurface4Impl_SetPalette,
    IDirectDrawSurface4Impl_Unlock,
    IDirectDrawSurface4Impl_UpdateOverlay,
    IDirectDrawSurface4Impl_UpdateOverlayDisplay,
    IDirectDrawSurface4Impl_UpdateOverlayZOrder,
    IDirectDrawSurface4Impl_GetDDInterface,
    IDirectDrawSurface4Impl_PageLock,
    IDirectDrawSurface4Impl_PageUnlock,
    IDirectDrawSurface4Impl_SetSurfaceDesc,
    IDirectDrawSurface4Impl_SetPrivateData,
    IDirectDrawSurface4Impl_GetPrivateData,
    IDirectDrawSurface4Impl_FreePrivateData,
    IDirectDrawSurface4Impl_GetUniquenessValue,
    IDirectDrawSurface4Impl_ChangeUniquenessValue
};
