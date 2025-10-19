#ifndef __PCF_HPP
#define __PCF_HPP

// #define UNICODE
// #define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE
#define NOMINMAX
#define STRICT


#define DXGI_DEBUG_INFO		// DXGI에서 발생한 예외 정보들을 출력할 경우 활성화

#include <Windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include <wrl.h>

#include <iostream>
#include <string>
#include <vector>
#include <list>

#undef min
#undef max
#undef near
#undef far

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


#endif	// __PCF_HPP