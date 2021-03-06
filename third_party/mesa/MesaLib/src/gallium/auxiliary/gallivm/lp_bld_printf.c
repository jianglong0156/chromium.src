/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <stdio.h>

#include "util/u_debug.h"
#include "util/u_memory.h"
#include "lp_bld_printf.h"


static int
lp_get_printf_arg_count(const char *fmt)
{
   int count =0;
   const char *p = fmt;
   int c;

   while ((c = *p++)) {
      if (c != '%')
         continue;
      switch (*p) {
         case '\0':
	    continue;
         case '%':
	    p++;
	    continue;
	 case '.':
	    if (p[1] == '*' && p[2] == 's') {
	       count += 2;
	       p += 3;
               continue;
	    }
	    /* fallthrough */
	 default:
	    count ++;
      }
   }
   return count;
}

LLVMValueRef 
lp_build_const_string_variable(LLVMModuleRef module, const char *str, int len)
{
   LLVMValueRef string = LLVMAddGlobal(module, LLVMArrayType(LLVMInt8Type(), len + 1), "");
   LLVMSetGlobalConstant(string, TRUE);
   LLVMSetLinkage(string, LLVMInternalLinkage);
   LLVMSetInitializer(string, LLVMConstString(str, len + 1, TRUE));
   return string;
}
 

/**
 * lp_build_printf.
 *
 * Build printf call in LLVM IR. The output goes to stdout.
 * The additional variable arguments need to have type
 * LLVMValueRef.
 */
LLVMValueRef
lp_build_printf(LLVMBuilderRef builder, const char *fmt, ...)
{
   va_list arglist;
   int i = 0;
   int argcount = lp_get_printf_arg_count(fmt);
   LLVMModuleRef module = LLVMGetGlobalParent(LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)));
   LLVMValueRef params[50];
   LLVMValueRef fmtarg = lp_build_const_string_variable(module, fmt, strlen(fmt) + 1);
   LLVMValueRef int0 = LLVMConstInt(LLVMInt32Type(), 0, 0);
   LLVMValueRef index[2];
   LLVMValueRef func_printf = LLVMGetNamedFunction(module, "printf");

   assert(Elements(params) >= argcount + 1);

   index[0] = index[1] = int0;

   if (!func_printf) {
      LLVMTypeRef printf_type = LLVMFunctionType(LLVMIntType(32), NULL, 0, 1);
      func_printf = LLVMAddFunction(module, "printf", printf_type);
   }

   params[0] = LLVMBuildGEP(builder, fmtarg, index, 2, "");

   va_start(arglist, fmt);
   for (i = 1; i <= argcount; i++) {
      LLVMValueRef val = va_arg(arglist, LLVMValueRef);
      LLVMTypeRef type = LLVMTypeOf(val);
      /* printf wants doubles, so lets convert so that
       * we can actually print them */
      if (LLVMGetTypeKind(type) == LLVMFloatTypeKind)
         val = LLVMBuildFPExt(builder, val, LLVMDoubleType(), "");
      params[i] = val;
   }
   va_end(arglist);

   return LLVMBuildCall(builder, func_printf, params, argcount + 1, "");
}

