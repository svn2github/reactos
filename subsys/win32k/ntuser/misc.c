/* $Id: misc.c,v 1.63 2004/04/25 20:05:30 weiden Exp $
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * PURPOSE:          Misc User funcs
 * FILE:             subsys/win32k/ntuser/misc.c
 * PROGRAMER:        Ge van Geldorp (ge@gse.nl)
 * REVISION HISTORY:
 *       2003/05/22  Created
 */
#include <ddk/ntddk.h>
#include <ddk/ntddmou.h>
#include <win32k/win32k.h>
#include <win32k/dc.h>
#include <internal/safe.h>
#include <include/error.h>
#include <include/window.h>
#include <include/menu.h>
#include <include/painting.h>
#include <include/dce.h>
#include <include/inteng.h>
#include <include/intgdi.h>
#include <include/mouse.h>
#include <include/winsta.h>
#include <include/caret.h>
#include <include/object.h>
#include <include/focus.h>
#include <include/clipboard.h>
#include <include/msgqueue.h>
#include <include/desktop.h>
#include <include/text.h>
#include <include/tags.h>

#define NDEBUG
#include <debug.h>

void W32kRegisterPrimitiveMessageQueue() {
  extern PUSER_MESSAGE_QUEUE pmPrimitiveMessageQueue;
  if( !pmPrimitiveMessageQueue ) {
    PW32THREAD pThread;
    pThread = PsGetWin32Thread();
    if( pThread && pThread->MessageQueue ) {
      pmPrimitiveMessageQueue = pThread->MessageQueue;
      DPRINT( "Installed primitive input queue.\n" );
    }    
  } else {
    DPRINT1( "Alert! Someone is trying to steal the primitive queue.\n" );
  }
}

PUSER_MESSAGE_QUEUE W32kGetPrimitiveMessageQueue() {
  extern PUSER_MESSAGE_QUEUE pmPrimitiveMessageQueue;
  return pmPrimitiveMessageQueue;
}

/*
 * @unimplemented
 */
DWORD
STDCALL
NtUserCallNoParam(DWORD Routine)
{
  DWORD Result = 0;

  switch(Routine)
  {
    case NOPARAM_ROUTINE_REGISTER_PRIMITIVE:
      W32kRegisterPrimitiveMessageQueue();
      Result = (DWORD)TRUE;
      break;
    
    case NOPARAM_ROUTINE_DESTROY_CARET:
      Result = (DWORD)IntDestroyCaret(PsGetCurrentThread()->Win32Thread);
      break;
    
    case NOPARAM_ROUTINE_INIT_MESSAGE_PUMP:
      Result = (DWORD)IntInitMessagePumpHook();
      break;

    case NOPARAM_ROUTINE_UNINIT_MESSAGE_PUMP:
      Result = (DWORD)IntUninitMessagePumpHook();
      break;
    
    case NOPARAM_ROUTINE_GETMESSAGEEXTRAINFO:
      Result = (DWORD)MsqGetMessageExtraInfo();
      break;

    default:
      DPRINT1("Calling invalid routine number 0x%x in NtUserCallTwoParam\n");
      SetLastWin32Error(ERROR_INVALID_PARAMETER);
      break;
  }
  return Result;
}

/*
 * @implemented
 */
DWORD
STDCALL
NtUserCallOneParam(
  DWORD Param,
  DWORD Routine)
{
  NTSTATUS Status;
  DWORD Result = 0;
  PWINSTATION_OBJECT WinStaObject;
  PWINDOW_OBJECT WindowObject;
  
  switch(Routine)
  {
    case ONEPARAM_ROUTINE_GETMENU:
      WindowObject = IntGetWindowObject((HWND)Param);
      if(!WindowObject)
      {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        return FALSE;
      }
      
      Result = (DWORD)WindowObject->IDMenu;
      
      IntReleaseWindowObject(WindowObject);
      return Result;
      
    case ONEPARAM_ROUTINE_ISWINDOWUNICODE:
      WindowObject = IntGetWindowObject((HWND)Param);
      if(!WindowObject)
      {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        return FALSE;
      }
      Result = WindowObject->Unicode;
      IntReleaseWindowObject(WindowObject);
      return Result;
      
    case ONEPARAM_ROUTINE_WINDOWFROMDC:
      return (DWORD)IntWindowFromDC((HDC)Param);
      
    case ONEPARAM_ROUTINE_GETWNDCONTEXTHLPID:
      WindowObject = IntGetWindowObject((HWND)Param);
      if(!WindowObject)
      {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        return FALSE;
      }
      
      Result = WindowObject->ContextHelpId;
      
      IntReleaseWindowObject(WindowObject);
      return Result;
    
    case ONEPARAM_ROUTINE_SWAPMOUSEBUTTON:
      Status = IntValidateWindowStationHandle(PROCESS_WINDOW_STATION(),
                                              KernelMode,
                                              0,
                                              &WinStaObject);
      if (!NT_SUCCESS(Status))
        return (DWORD)FALSE;

      Result = (DWORD)IntSwapMouseButton(WinStaObject, (BOOL)Param);

      ObDereferenceObject(WinStaObject);
      return Result;
    
    case ONEPARAM_ROUTINE_SWITCHCARETSHOWING:
      return (DWORD)IntSwitchCaretShowing((PVOID)Param);
    
    case ONEPARAM_ROUTINE_SETCARETBLINKTIME:
      return (DWORD)IntSetCaretBlinkTime((UINT)Param);

    case ONEPARAM_ROUTINE_ENUMCLIPBOARDFORMATS:
      return (DWORD)IntEnumClipboardFormats((UINT)Param);
    
    case ONEPARAM_ROUTINE_GETWINDOWINSTANCE:
      if(!(WindowObject = IntGetWindowObject((HWND)Param)))
      {
        SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
        return FALSE;
      }
      
      Result = (DWORD)WindowObject->Instance;
      IntReleaseWindowObject(WindowObject);
      return Result;
    
    case ONEPARAM_ROUTINE_SETMESSAGEEXTRAINFO:
      return (DWORD)MsqSetMessageExtraInfo((LPARAM)Param);
  }
  DPRINT1("Calling invalid routine number 0x%x in NtUserCallOneParam()\n Param=0x%x\n", 
          Routine, Param);
  SetLastWin32Error(ERROR_INVALID_PARAMETER);
  return 0;
}


/*
 * @implemented
 */
DWORD
STDCALL
NtUserCallTwoParam(
  DWORD Param1,
  DWORD Param2,
  DWORD Routine)
{
  NTSTATUS Status;
  PWINDOW_OBJECT WindowObject;
  PSYSTEM_CURSORINFO CurInfo;
  PWINSTATION_OBJECT WinStaObject;
  POINT Pos;
  
  switch(Routine)
  {
    case TWOPARAM_ROUTINE_SETDCPENCOLOR:
    {
      return (DWORD)IntSetDCColor((HDC)Param1, OBJ_PEN, (COLORREF)Param2);
    }
    case TWOPARAM_ROUTINE_SETDCBRUSHCOLOR:
    {
      return (DWORD)IntSetDCColor((HDC)Param1, OBJ_BRUSH, (COLORREF)Param2);
    }
    case TWOPARAM_ROUTINE_GETDCCOLOR:
    {
      return (DWORD)IntGetDCColor((HDC)Param1, (ULONG)Param2);
    }
    case TWOPARAM_ROUTINE_GETWINDOWRGNBOX:
    {
      DWORD Ret;
      RECT rcRect;
      Ret = (DWORD)IntGetWindowRgnBox((HWND)Param1, &rcRect);
      Status = MmCopyToCaller((PVOID)Param2, &rcRect, sizeof(RECT));
      if(!NT_SUCCESS(Status))
      {
        SetLastNtError(Status);
        return ERROR;
      }
      return Ret;
    }
    case TWOPARAM_ROUTINE_GETWINDOWRGN:
    {
      return (DWORD)IntGetWindowRgn((HWND)Param1, (HRGN)Param2);
    }
    case TWOPARAM_ROUTINE_SETMENUBARHEIGHT:
    {
      DWORD Ret;
      PMENU_OBJECT MenuObject = IntGetMenuObject((HMENU)Param1);
      if(!MenuObject)
        return 0;
      
      if(Param2 > 0)
      {
        Ret = (MenuObject->MenuInfo.Height == (int)Param2);
        MenuObject->MenuInfo.Height = (int)Param2;
      }
      else
        Ret = (DWORD)MenuObject->MenuInfo.Height;
      IntReleaseMenuObject(MenuObject);
      return Ret;
    }
    case TWOPARAM_ROUTINE_SETMENUITEMRECT:
    {
      BOOL Ret;
      SETMENUITEMRECT smir;
      PMENU_OBJECT MenuObject = IntGetMenuObject((HMENU)Param1);
      if(!MenuObject)
        return 0;
      
      if(!NT_SUCCESS(MmCopyFromCaller(&smir, (PVOID)Param2, sizeof(SETMENUITEMRECT))))
      {
        IntReleaseMenuObject(MenuObject);
        return 0;
      }
      
      Ret = IntSetMenuItemRect(MenuObject, smir.uItem, smir.fByPosition, &smir.rcRect);
      
      IntReleaseMenuObject(MenuObject);
      return (DWORD)Ret;
    }
    
    case TWOPARAM_ROUTINE_SETGUITHRDHANDLE:
    {
      PUSER_MESSAGE_QUEUE MsgQueue = PsGetCurrentThread()->Win32Thread->MessageQueue;
      
      ASSERT(MsgQueue);
      return (DWORD)MsqSetStateWindow(MsgQueue, (ULONG)Param1, (HWND)Param2);
    }
    
    case TWOPARAM_ROUTINE_ENABLEWINDOW:
	  UNIMPLEMENTED
      return 0;

    case TWOPARAM_ROUTINE_UNKNOWN:
	  UNIMPLEMENTED
	  return 0;

    case TWOPARAM_ROUTINE_SHOWOWNEDPOPUPS:
	  UNIMPLEMENTED
	  return 0;

    case TWOPARAM_ROUTINE_SWITCHTOTHISWINDOW:
	  UNIMPLEMENTED
	  return 0;

    case TWOPARAM_ROUTINE_VALIDATERGN:
      return (DWORD)NtUserValidateRgn((HWND) Param1, (HRGN) Param2);
      
    case TWOPARAM_ROUTINE_SETWNDCONTEXTHLPID:
      WindowObject = IntGetWindowObject((HWND)Param1);
      if(!WindowObject)
      {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        return (DWORD)FALSE;
      }
      
      WindowObject->ContextHelpId = Param2;
      
      IntReleaseWindowObject(WindowObject);
      return (DWORD)TRUE;
      
    case TWOPARAM_ROUTINE_CURSORPOSITION:
      if(!Param1)
        return (DWORD)FALSE;
      Status = IntValidateWindowStationHandle(PROCESS_WINDOW_STATION(),
                                              KernelMode,
                                              0,
                                              &WinStaObject);
      if (!NT_SUCCESS(Status))
        return (DWORD)FALSE;
      
      if(Param2)
      {
        /* set cursor position */
        
        Status = MmCopyFromCaller(&Pos, (PPOINT)Param1, sizeof(POINT));
        if(!NT_SUCCESS(Status))
        {
          ObDereferenceObject(WinStaObject);
          SetLastNtError(Status);
          return FALSE;
        }
        
        CurInfo = &WinStaObject->SystemCursor;
        /* FIXME - check if process has WINSTA_WRITEATTRIBUTES */
        
        //CheckClipCursor(&Pos->x, &Pos->y, CurInfo);  
        if((Pos.x != CurInfo->x) || (Pos.y != CurInfo->y))
        {
          MouseMoveCursor(Pos.x, Pos.y);
        }

      }
      else
      {
        /* get cursor position */
        /* FIXME - check if process has WINSTA_READATTRIBUTES */
        Pos.x = WinStaObject->SystemCursor.x;
        Pos.y = WinStaObject->SystemCursor.y;
        
        Status = MmCopyToCaller((PPOINT)Param1, &Pos, sizeof(POINT));
        if(!NT_SUCCESS(Status))
        {
          ObDereferenceObject(WinStaObject);
          SetLastNtError(Status);
          return FALSE;
        }
        
      }
      
      ObDereferenceObject(WinStaObject);
      
      return (DWORD)TRUE;
    
    case TWOPARAM_ROUTINE_SETCARETPOS:
      return (DWORD)IntSetCaretPos((int)Param1, (int)Param2);
    
    case TWOPARAM_ROUTINE_GETWINDOWINFO:
    {
      WINDOWINFO wi;
      DWORD Ret;
      
      if(!(WindowObject = IntGetWindowObject((HWND)Param1)))
      {
        SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
        return FALSE;
      }
      
      Status = MmCopyFromCaller(&wi.cbSize, (PVOID)Param2, sizeof(wi.cbSize));
      if(!NT_SUCCESS(Status))
      {
        IntReleaseWindowObject(WindowObject);
        SetLastNtError(Status);
        return FALSE;
      }
      
      if(wi.cbSize != sizeof(WINDOWINFO))
      {
        IntReleaseWindowObject(WindowObject);
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        return FALSE;
      }
      
      if((Ret = (DWORD)IntGetWindowInfo(WindowObject, &wi)))
      {
        Status = MmCopyToCaller((PVOID)Param2, &wi, sizeof(WINDOWINFO));
        if(!NT_SUCCESS(Status))
        {
          IntReleaseWindowObject(WindowObject);
          SetLastNtError(Status);
          return FALSE;
        }
      }
      
      IntReleaseWindowObject(WindowObject);
      return Ret;
    }
  }
  DPRINT1("Calling invalid routine number 0x%x in NtUserCallTwoParam()\n Param1=0x%x Parm2=0x%x\n",
          Routine, Param1, Param2);
  SetLastWin32Error(ERROR_INVALID_PARAMETER);
  return 0;
}

/*
 * @unimplemented
 */
BOOL
STDCALL
NtUserCallHwndLock(
  HWND hWnd,
  DWORD Routine)
{
   BOOL Ret = 0;
   PWINDOW_OBJECT Window;

   Window = IntGetWindowObject(hWnd);
   if (Window == 0)
   {
      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
      return FALSE;
   }

   /* FIXME: Routine can be 0x53 - 0x5E */
   switch (Routine)
   {
      case HWNDLOCK_ROUTINE_ARRANGEICONICWINDOWS:
         /* FIXME */
         break;

      case HWNDLOCK_ROUTINE_DRAWMENUBAR:
         /* FIXME */
         break;

      case HWNDLOCK_ROUTINE_REDRAWFRAME:
         /* FIXME */
         break;

      case HWNDLOCK_ROUTINE_SETFOREGROUNDWINDOW:         
         Ret = IntSetForegroundWindow(Window);
         break;

      case HWNDLOCK_ROUTINE_UPDATEWINDOW:
         /* FIXME */
         break;
   }

   IntReleaseWindowObject(Window);

   return Ret;
}

HWND
STDCALL
NtUserCallHwndOpt(
  HWND Param,
  DWORD Routine)
{
   switch (Routine)
   {
      case HWNDOPT_ROUTINE_SETPROGMANWINDOW:
         /* FIXME */
         break;

      case HWNDOPT_ROUTINE_SETTASKMANWINDOW:
         /* FIXME */
         break;
   }

   return 0;
}

/*
 * @unimplemented
 */
DWORD STDCALL
NtUserGetThreadState(
  DWORD Routine)
{
   switch (Routine)
   {
      case 0:
         return (DWORD)IntGetThreadFocusWindow();
   }
   return 0;
}

VOID FASTCALL
IntGetFontMetricSetting(LPWSTR lpValueName, PLOGFONTW font) 
{ 
   RTL_QUERY_REGISTRY_TABLE QueryTable[2];
   NTSTATUS Status;
   static LOGFONTW DefaultFont = {
      11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
      0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS,
      L"Bitstream Vera Sans"
   };

   RtlZeroMemory(&QueryTable, sizeof(QueryTable));

   QueryTable[0].Name = lpValueName;
   QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
   QueryTable[0].EntryContext = font;

   Status = RtlQueryRegistryValues(
      RTL_REGISTRY_USER,
      L"Control Panel\\Desktop\\WindowMetrics",
      QueryTable,
      NULL,
      NULL);

   if (!NT_SUCCESS(Status))
   {
      RtlCopyMemory(font, &DefaultFont, sizeof(LOGFONTW));
   }
}

/*
 * @implemented
 */
DWORD
STDCALL
NtUserSystemParametersInfo(
  UINT uiAction,
  UINT uiParam,
  PVOID pvParam,
  UINT fWinIni)
{
  static BOOL GradientCaptions = TRUE;
  NTSTATUS Status;
  PWINSTATION_OBJECT WinStaObject;

  static BOOL bInitialized = FALSE;
  static LOGFONTW IconFont;
  static NONCLIENTMETRICSW pMetrics;
  
  if (!bInitialized)
  {
    ZeroMemory(&IconFont, sizeof(LOGFONTW)); 
    ZeroMemory(&pMetrics, sizeof(NONCLIENTMETRICSW));
    
    IntGetFontMetricSetting(L"CaptionFont", &pMetrics.lfCaptionFont);
    IntGetFontMetricSetting(L"SmCaptionFont", &pMetrics.lfSmCaptionFont);
    IntGetFontMetricSetting(L"MenuFont", &pMetrics.lfMenuFont);
    IntGetFontMetricSetting(L"StatusFont", &pMetrics.lfStatusFont);
    IntGetFontMetricSetting(L"MessageFont", &pMetrics.lfMessageFont);
    IntGetFontMetricSetting(L"IconFont", &IconFont);
    
    pMetrics.iBorderWidth = 1;
    pMetrics.iScrollWidth = NtUserGetSystemMetrics(SM_CXVSCROLL);
    pMetrics.iScrollHeight = NtUserGetSystemMetrics(SM_CYHSCROLL);
    pMetrics.iCaptionWidth = NtUserGetSystemMetrics(SM_CXSIZE);
    pMetrics.iCaptionHeight = NtUserGetSystemMetrics(SM_CYSIZE);
    pMetrics.iSmCaptionWidth = NtUserGetSystemMetrics(SM_CXSMSIZE);
    pMetrics.iSmCaptionHeight = NtUserGetSystemMetrics(SM_CYSMSIZE);
    pMetrics.iMenuWidth = NtUserGetSystemMetrics(SM_CXMENUSIZE);
    pMetrics.iMenuHeight = NtUserGetSystemMetrics(SM_CYMENUSIZE);
    pMetrics.cbSize = sizeof(LPNONCLIENTMETRICSW);
    
    bInitialized = TRUE;
  }

  switch(uiAction)
  {
    case SPI_SETDOUBLECLKWIDTH:
    case SPI_SETDOUBLECLKHEIGHT:
    case SPI_SETDOUBLECLICKTIME:
      {
        Status = IntValidateWindowStationHandle(PROCESS_WINDOW_STATION(),
                                                KernelMode,
                                                0,
                                                &WinStaObject);
        if (!NT_SUCCESS(Status))
          return (DWORD)FALSE;
        
        switch(uiAction)
        {
          case SPI_SETDOUBLECLKWIDTH:
            /* FIXME limit the maximum value? */
            WinStaObject->SystemCursor.DblClickWidth = uiParam;
            break;
          case SPI_SETDOUBLECLKHEIGHT:
            /* FIXME limit the maximum value? */
            WinStaObject->SystemCursor.DblClickHeight = uiParam;
            break;
          case SPI_SETDOUBLECLICKTIME:
            /* FIXME limit the maximum time to 1000 ms? */
            WinStaObject->SystemCursor.DblClickSpeed = uiParam;
            break;
        }
        
        /* FIXME save the value to the registry */
        
        ObDereferenceObject(WinStaObject);
        return TRUE;
      }
    case SPI_SETWORKAREA:
      {
        PDESKTOP_OBJECT Desktop = PsGetWin32Thread()->Desktop;
        
        if(!Desktop)
        {
          /* FIXME - Set last error */
          return FALSE;
        }
        
        Status = MmCopyFromCaller(Desktop->WorkArea, (PRECT)pvParam, sizeof(RECT));
        if(!NT_SUCCESS(Status))
        {
          SetLastNtError(Status);
          return FALSE;
        }
        
        return TRUE;
      }
    case SPI_GETWORKAREA:
      {
        PRECT Rect;
        PDESKTOP_OBJECT Desktop = PsGetWin32Thread()->Desktop;
        
        if(!Desktop)
        {
          /* FIXME - Set last error */
          return FALSE;
        }
        
        Rect = IntGetDesktopWorkArea(Desktop);
        
        Status = MmCopyToCaller((PRECT)pvParam, Desktop->WorkArea, sizeof(RECT));
        if(!NT_SUCCESS(Status))
        {
          SetLastNtError(Status);
          return FALSE;
        }
        
        return TRUE;
      }
    case SPI_GETGRADIENTCAPTIONS:
      {
        HDC hDC;
        PDC dc;
        SURFOBJ *SurfObj;
        BOOL Ret = GradientCaptions;
        
        hDC = IntGetScreenDC();
        if(hDC)
        {
          dc = DC_LockDc(hDC);
          SurfObj = (SURFOBJ*)AccessUserObject((ULONG) dc->Surface);
          if(SurfObj)
            Ret = (SurfObj->iBitmapFormat > BMF_8BPP);
          DC_UnlockDc(hDC);
        }
        
        Status = MmCopyToCaller(pvParam, &Ret, sizeof(BOOL));
        if(!NT_SUCCESS(Status))
        {
          SetLastNtError(Status);
          return FALSE;
        }
        return TRUE;
      }
    case SPI_SETGRADIENTCAPTIONS:
      {
        Status = MmCopyFromCaller(&GradientCaptions, pvParam, sizeof(BOOL));
        if(!NT_SUCCESS(Status))
        {
          SetLastNtError(Status);
          return FALSE;
        }
        return TRUE;
      }
    case SPI_SETFONTSMOOTHING:
      {
        BOOL Enable;
        
        Status = MmCopyFromCaller(&Enable, pvParam, sizeof(BOOL));
        if(!NT_SUCCESS(Status))
        {
          SetLastNtError(Status);
          return FALSE;
        }
        
        IntEnableFontRendering(Enable);
        
        return TRUE;
      }
    case SPI_GETFONTSMOOTHING:
      {
        BOOL Enabled = IntIsFontRenderingEnabled();
        
        Status = MmCopyToCaller(pvParam, &Enabled, sizeof(BOOL));
        if(!NT_SUCCESS(Status))
        {
          SetLastNtError(Status);
          return FALSE;
        }
        return TRUE;
      }
    case SPI_GETICONTITLELOGFONT:
    {
        MmCopyToCaller(pvParam, (PVOID)&IconFont, sizeof(LOGFONTW));
        return TRUE;
    } 
    case SPI_GETNONCLIENTMETRICS:
    {
        /* FIXME:  Is this windows default behavior? */
        LPNONCLIENTMETRICSW lpMetrics = (LPNONCLIENTMETRICSW)pvParam;
        if ( lpMetrics->cbSize != sizeof(NONCLIENTMETRICSW) || 
             uiParam != sizeof(NONCLIENTMETRICSW ))
        {
            return FALSE;
        }
        DPRINT("FontName: %S, Size:  %i\n",pMetrics.lfMessageFont.lfFaceName, pMetrics.lfMessageFont.lfHeight);
        MmCopyToCaller(pvParam, (PVOID)&pMetrics, sizeof(NONCLIENTMETRICSW));
        return TRUE;
    }
  }
  return FALSE;
}

UINT
STDCALL
NtUserGetDoubleClickTime(VOID)
{
  UINT Result;
  NTSTATUS Status;
  PWINSTATION_OBJECT WinStaObject;
  
  Status = IntValidateWindowStationHandle(PROCESS_WINDOW_STATION(),
                                          KernelMode,
                                          0,
                                          &WinStaObject);
  if (!NT_SUCCESS(Status))
    return (DWORD)FALSE;

  Result = WinStaObject->SystemCursor.DblClickSpeed;
      
  ObDereferenceObject(WinStaObject);
  return Result;
}

BOOL
STDCALL
NtUserGetGUIThreadInfo(
  DWORD idThread,
  LPGUITHREADINFO lpgui)
{
  NTSTATUS Status;
  PTHRDCARETINFO CaretInfo;
  GUITHREADINFO SafeGui;
  PDESKTOP_OBJECT Desktop;
  PUSER_MESSAGE_QUEUE MsgQueue;
  PETHREAD Thread = NULL;
  
  Status = MmCopyFromCaller(&SafeGui, lpgui, sizeof(DWORD));
  if(!NT_SUCCESS(Status))
  {
    SetLastNtError(Status);
    return FALSE;
  }
  
  if(SafeGui.cbSize != sizeof(GUITHREADINFO))
  {
    SetLastWin32Error(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  
  if(idThread)
  {
    Status = PsLookupThreadByThreadId((PVOID)idThread, &Thread);
    if(!NT_SUCCESS(Status))
    {
      SetLastWin32Error(ERROR_ACCESS_DENIED);
      return FALSE;
    }
    Desktop = Thread->Win32Thread->Desktop;
  }
  else
  {
    /* get the foreground thread */
    PW32THREAD W32Thread = PsGetCurrentThread()->Win32Thread;
    Desktop = W32Thread->Desktop;
    if(Desktop)
    {
      MsgQueue = Desktop->ActiveMessageQueue;
      if(MsgQueue)
      {
        Thread = MsgQueue->Thread;
      }
    }
  }
  
  if(!Thread || !Desktop)
  {
    if(idThread && Thread)
      ObDereferenceObject(Thread);
    SetLastWin32Error(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  
  MsgQueue = (PUSER_MESSAGE_QUEUE)Desktop->ActiveMessageQueue;
  CaretInfo = MsgQueue->CaretInfo;
  
  SafeGui.flags = (CaretInfo->Visible ? GUI_CARETBLINKING : 0);
  if(MsgQueue->MenuOwner)
    SafeGui.flags |= GUI_INMENUMODE | MsgQueue->MenuState;
  if(MsgQueue->MoveSize)
    SafeGui.flags |= GUI_INMOVESIZE;
  
  /* FIXME add flag GUI_16BITTASK */
  
  SafeGui.hwndActive = MsgQueue->ActiveWindow;
  SafeGui.hwndFocus = MsgQueue->FocusWindow;
  SafeGui.hwndCapture = MsgQueue->CaptureWindow;
  SafeGui.hwndMenuOwner = MsgQueue->MenuOwner;
  SafeGui.hwndMoveSize = MsgQueue->MoveSize;
  SafeGui.hwndCaret = CaretInfo->hWnd;
  
  SafeGui.rcCaret.left = CaretInfo->Pos.x;
  SafeGui.rcCaret.top = CaretInfo->Pos.y;
  SafeGui.rcCaret.right = SafeGui.rcCaret.left + CaretInfo->Size.cx;
  SafeGui.rcCaret.bottom = SafeGui.rcCaret.top + CaretInfo->Size.cy;
  
  if(idThread)
    ObDereferenceObject(Thread);
  
  Status = MmCopyToCaller(lpgui, &SafeGui, sizeof(GUITHREADINFO));
  if(!NT_SUCCESS(Status))
  {
    SetLastNtError(Status);
    return FALSE;
  }

  return TRUE;
}


DWORD
STDCALL
NtUserGetGuiResources(
  HANDLE hProcess,
  DWORD uiFlags)
{
  PEPROCESS Process;
  PW32PROCESS W32Process;
  NTSTATUS Status;
  DWORD Ret = 0;
  
  Status = ObReferenceObjectByHandle(hProcess,
                                     PROCESS_QUERY_INFORMATION,
                                     PsProcessType,
                                     ExGetPreviousMode(),
                                     (PVOID*)&Process,
                                     NULL);
  
  if(!NT_SUCCESS(Status))
  {
    SetLastNtError(Status);
    return 0;
  }
  
  W32Process = Process->Win32Process;
  if(!W32Process)
  {
    ObDereferenceObject(Process);
    SetLastWin32Error(ERROR_INVALID_PARAMETER);
    return 0;
  }
  
  switch(uiFlags)
  {
    case GR_GDIOBJECTS:
    {
      Ret = (DWORD)W32Process->GDIObjects;
      break;
    }
    case GR_USEROBJECTS:
    {
      Ret = (DWORD)W32Process->UserObjects;
      break;
    }
    default:
    {
      SetLastWin32Error(ERROR_INVALID_PARAMETER);
      break;
    }
  }
  
  ObDereferenceObject(Process);

  return Ret;
}

NTSTATUS FASTCALL
IntSafeCopyUnicodeString(PUNICODE_STRING Dest,
                         PUNICODE_STRING Source)
{
  NTSTATUS Status;
  PWSTR Src;
  
  Status = MmCopyFromCaller(Dest, Source, sizeof(UNICODE_STRING));
  if(!NT_SUCCESS(Status))
  {
    return Status;
  }
  
  if(Dest->Length > 0x4000)
  {
    return STATUS_UNSUCCESSFUL;
  }
  
  Src = Dest->Buffer;
  Dest->Buffer = NULL;
  
  if(Dest->Length > 0 && Src)
  {
    Dest->Buffer = ExAllocatePoolWithTag(NonPagedPool, Dest->Length, TAG_STRING);
    if(!Dest->Buffer)
    {
      return STATUS_NO_MEMORY;
    }
    
    Status = MmCopyFromCaller(Dest->Buffer, Src, Dest->Length);
    if(!NT_SUCCESS(Status))
    {
      ExFreePool(Dest->Buffer);
      Dest->Buffer = NULL;
      return Status;
    }
    
    
    return STATUS_SUCCESS;
  }
  
  /* string is empty */
  return STATUS_SUCCESS;
}

