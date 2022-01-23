; Nasty hack to get the hook to work without re-patching ique_diag.exe
; v0.4 and below define __declspec(dllexport) void UselessExport() from C++,
; which is mangled to ?UselessExport@@YAXXZ by the compiler.
; The patched ique_diag.exe looks for this function in iQueDiagExtend.dll
; in order to start the hooking process, so the hook won't work if it's missing.
; Annoyingly, emoose chose to use the mangled name for this purpose,
; instead of doing extern "C" and using the unmangled name, which means there
; needs to be an exported function in the DLL called ?UselessExport@@YAXXZ.
; Unfortunately, this isn't a valid C identifier, and MSVC's workaround
; (__identifier("?UselessExport@@YAXXZ")) is C++-only and doesn't work in C.
; Therefore, I found this workaround: writing the function (which is a dummy
; and can immediately return) in assembly and including it in the build.
; This is irritating to get working in the MSVC build process, but I think
; it's stable now.

_text	SEGMENT
?UselessExport@@YAXXZ PROC FAR EXPORT
	ret	0
?UselessExport@@YAXXZ ENDP
_text	ENDS

END
