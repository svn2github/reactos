/*
 * PROJECT:         ReactOS win32 kernel mode subsystem
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            subsystems/win32/win32k/objects/text.c
 * PURPOSE:         Text/Font
 * PROGRAMMER:      
 */
      
/** Includes ******************************************************************/

#include <w32k.h>

#define NDEBUG
#include <debug.h>

BOOL
FASTCALL
ftGdiGetRasterizerCaps(LPRASTERIZER_STATUS lprs);

BOOL
FASTCALL
TextIntGetTextExtentPoint(PDC dc,
                          PTEXTOBJ TextObj,
                          LPCWSTR String,
                          int Count,
                          int MaxExtent,
                          LPINT Fit,
                          LPINT Dx,
                          LPSIZE Size);


/** Functions ******************************************************************/

BOOL
APIENTRY
NtGdiGetRasterizerCaps(
    OUT LPRASTERIZER_STATUS praststat,
    IN ULONG cjBytes)
{
  NTSTATUS Status = STATUS_SUCCESS;
  RASTERIZER_STATUS rsSafe;

  if (praststat && cjBytes)
  {
     if ( cjBytes >= sizeof(RASTERIZER_STATUS) ) cjBytes = sizeof(RASTERIZER_STATUS);
     if ( ftGdiGetRasterizerCaps(&rsSafe))
     {
        _SEH_TRY
        {
           ProbeForWrite( praststat,
                          sizeof(RASTERIZER_STATUS),
                          1);
           RtlCopyMemory(praststat, &rsSafe, cjBytes );
        }
        _SEH_HANDLE
        {
           Status = _SEH_GetExceptionCode();
        }
        _SEH_END;

        if (!NT_SUCCESS(Status))
        {
           SetLastNtError(Status);
           return FALSE;
        }
     }
  }
  return FALSE;
}

W32KAPI
BOOL
APIENTRY
NtGdiGetTextExtentExW(
    IN HDC hDC,
    IN OPTIONAL LPWSTR UnsafeString,
    IN ULONG Count,
    IN ULONG MaxExtent,
    OUT OPTIONAL PULONG UnsafeFit,
    OUT OPTIONAL PULONG UnsafeDx,
    OUT LPSIZE UnsafeSize,
    IN FLONG fl
)
{
  PDC dc;
  PDC_ATTR Dc_Attr;
  LPWSTR String;
  SIZE Size;
  NTSTATUS Status;
  BOOLEAN Result;
  INT Fit;
  LPINT Dx;
  PTEXTOBJ TextObj;

  /* FIXME: Handle fl */

  if (0 == Count)
    {
      Size.cx = 0;
      Size.cy = 0;
      Status = MmCopyToCaller(UnsafeSize, &Size, sizeof(SIZE));
      if (! NT_SUCCESS(Status))
	{
	  SetLastNtError(Status);
	  return FALSE;
	}
      return TRUE;
    }

  String = ExAllocatePoolWithTag(PagedPool, Count * sizeof(WCHAR), TAG_GDITEXT);
  if (NULL == String)
    {
      SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
    }

  if (NULL != UnsafeDx)
    {
      Dx = ExAllocatePoolWithTag(PagedPool, Count * sizeof(INT), TAG_GDITEXT);
      if (NULL == Dx)
	{
	  ExFreePool(String);
	  SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
	  return FALSE;
	}
    }
  else
    {
      Dx = NULL;
    }

  Status = MmCopyFromCaller(String, UnsafeString, Count * sizeof(WCHAR));
  if (! NT_SUCCESS(Status))
    {
      if (NULL != Dx)
	{
	  ExFreePool(Dx);
	}
      ExFreePool(String);
      SetLastNtError(Status);
      return FALSE;
    }

  dc = DC_LockDc(hDC);
  if (NULL == dc)
    {
      if (NULL != Dx)
	{
	  ExFreePool(Dx);
	}
      ExFreePool(String);
      SetLastWin32Error(ERROR_INVALID_HANDLE);
      return FALSE;
    }
  Dc_Attr = dc->pDc_Attr;
  if(!Dc_Attr) Dc_Attr = &dc->Dc_Attr;
  TextObj = TEXTOBJ_LockText(Dc_Attr->hlfntNew);
  if ( TextObj )
  {
    Result = TextIntGetTextExtentPoint(dc, TextObj, String, Count, MaxExtent,
                                     NULL == UnsafeFit ? NULL : &Fit, Dx, &Size);
    TEXTOBJ_UnlockText(TextObj);
  }
  else
    Result = FALSE;
  DC_UnlockDc(dc);

  ExFreePool(String);
  if (! Result)
    {
      if (NULL != Dx)
	{
	  ExFreePool(Dx);
	}
      return FALSE;
    }

  if (NULL != UnsafeFit)
    {
      Status = MmCopyToCaller(UnsafeFit, &Fit, sizeof(INT));
      if (! NT_SUCCESS(Status))
	{
	  if (NULL != Dx)
	    {
	      ExFreePool(Dx);
	    }
	  SetLastNtError(Status);
	  return FALSE;
	}
    }

  if (NULL != UnsafeDx)
    {
      Status = MmCopyToCaller(UnsafeDx, Dx, Count * sizeof(INT));
      if (! NT_SUCCESS(Status))
	{
	  if (NULL != Dx)
	    {
	      ExFreePool(Dx);
	    }
	  SetLastNtError(Status);
	  return FALSE;
	}
    }
  if (NULL != Dx)
    {
      ExFreePool(Dx);
    }

  Status = MmCopyToCaller(UnsafeSize, &Size, sizeof(SIZE));
  if (! NT_SUCCESS(Status))
    {
      SetLastNtError(Status);
      return FALSE;
    }

  return TRUE;
}

BOOL
STDCALL
NtGdiGetTextExtent(HDC hdc,
                   LPWSTR lpwsz,
                   INT cwc,
                   LPSIZE psize,
                   UINT flOpts)
{
  return NtGdiGetTextExtentExW(hdc, lpwsz, cwc, 0, NULL, NULL, psize, 0);
}

BOOL
STDCALL
NtGdiSetTextJustification(HDC  hDC,
                          int  BreakExtra,
                          int  BreakCount)
{
  PDC pDc;
  PDC_ATTR pDc_Attr;

  pDc = DC_LockDc(hDC);
  if (!pDc)
  {
     SetLastWin32Error(ERROR_INVALID_HANDLE);
     return FALSE;
  }

  pDc_Attr = pDc->pDc_Attr;
  if(!pDc_Attr) pDc_Attr = &pDc->Dc_Attr;

  pDc_Attr->lBreakExtra = BreakExtra;
  pDc_Attr->cBreak = BreakCount;

  DC_UnlockDc(pDc);
  return TRUE;
}


W32KAPI
INT
APIENTRY
NtGdiGetTextFaceW(
    IN HDC hDC,
    IN INT Count,
    OUT OPTIONAL LPWSTR FaceName,
    IN BOOL bAliasName
)
{
   PDC Dc;
   PDC_ATTR Dc_Attr;
   HFONT hFont;
   PTEXTOBJ TextObj;
   NTSTATUS Status;

   /* FIXME: Handle bAliasName */

   Dc = DC_LockDc(hDC);
   if (Dc == NULL)
   {
      SetLastWin32Error(ERROR_INVALID_HANDLE);
      return FALSE;
   }
   Dc_Attr = Dc->pDc_Attr;
   if(!Dc_Attr) Dc_Attr = &Dc->Dc_Attr;
   hFont = Dc_Attr->hlfntNew;
   DC_UnlockDc(Dc);

   TextObj = TEXTOBJ_LockText(hFont);
   ASSERT(TextObj != NULL);
   Count = min(Count, wcslen(TextObj->logfont.elfEnumLogfontEx.elfLogFont.lfFaceName));
   Status = MmCopyToCaller(FaceName, TextObj->logfont.elfEnumLogfontEx.elfLogFont.lfFaceName, Count * sizeof(WCHAR));
   TEXTOBJ_UnlockText(TextObj);
   if (!NT_SUCCESS(Status))
   {
      SetLastNtError(Status);
      return 0;
   }

   return Count;
}

/* EOF */
